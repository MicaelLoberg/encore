#define __STDC_FORMAT_MACROS
#include <pony/pony.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include <assert.h>
#include "future_actor.h"
#include "future.h"
#include "context.h"
#include "set.h"

extern volatile bool hacky_global_blocked_flag;

typedef struct future_actor_fields {
  Set blocked;
  Set chained;
  Set yielded;
} future_actor_fields;

typedef struct chained_entry {
  pony_actor_t *actor;
  void *closure;
} chained_entry;

typedef struct blocked_entry {
  pony_actor_t *actor;
  Ctx context;
} blocked_entry;

pony_actor_type_t future_actor_type =
{
  1,
  {NULL, sizeof(future_actor_fields), PONY_ACTOR},
  future_actor_message_type,
  future_actor_dispatch
};


// FIXME -- 2nd arg in chain should be a closure, not an actor!
static pony_msg_t m_chain = {2,
			     {
			       {NULL, 0, PONY_ACTOR},
			       {NULL, 0, PONY_ACTOR}}};
// FIXME -- 2nd arg should be Ctx defined in context.h
static pony_msg_t m_yield = {2,
			     {
			       {NULL, 0, PONY_ACTOR},
			       {NULL, 0, PONY_ACTOR}}};
// FIXME -- 2nd arg should be Ctx defined in context.h
static pony_msg_t m_block = {2,
			     {
			       {NULL, 0, PONY_ACTOR},
			       {NULL, 0, PONY_ACTOR}}};
// FIXME -- 2nd arg in chain should be any kind of Encore value
static pony_msg_t m_fulfil = {1, {{NULL, 0, PONY_ACTOR}}};
// FIXME -- arg should be Ctx defined in context.h
static pony_msg_t m_resume_get = {1, {{NULL, 0, PONY_ACTOR}} };

pony_msg_t* future_actor_message_type(uint64_t id) {
  switch (id) {
  case FUT_MSG_CHAIN: return &m_chain;
  case FUT_MSG_BLOCK: return &m_block;
  case FUT_MSG_YIELD: return &m_yield;
  case FUT_MSG_FULFIL: return &m_fulfil;
  case FUT_MSG_RESUME: return &m_resume_get;
  }

  return NULL;
}

static void init(pony_actor_t* this) {
  if (this->p) return;
  pony_set(pony_alloc(sizeof (future_actor_fields)));
  // FIXME: are the following inits really necessary? (is memory nulled from start?)
  future_actor_fields *fields = this->p;
  fields->blocked = NULL;
  fields->chained = NULL;
  fields->yielded = NULL;
}

static Set getBlocked(pony_actor_t* this) {
  future_actor_fields *fields = this->p;
  if (fields->blocked == NULL) {
    fields->blocked = set_new();
  }
  return fields->blocked;
}

static Set getChained(pony_actor_t* this) {
  future_actor_fields *fields = this->p;
  if (fields->chained == NULL) {
    fields->chained = set_new();
  }
  return fields->chained;
}

static Set getYielded(pony_actor_t* this) {
  future_actor_fields *fields = this->p;
  if (fields->yielded == NULL) {
    fields->yielded = set_new();
  }
  return fields->yielded;
}

static void resume(blocked_entry *entry) {
  pony_actor_t *target = entry->actor;
  Ctx context_to_resume = entry->context;
  pony_arg_t argv[1];
  argv[0].p = context_to_resume;
  pony_sendv(target, FUT_MSG_RESUME, 1, argv);
}

static void run_chain(chained_entry *entry) {
  pony_actor_t *target = entry->actor;
  void *closure = entry->closure;
  pony_arg_t argv[1];
  argv[0].p = closure;
  pony_sendv(target, FUT_MSG_RUN_CLOSURE, 1, argv); // - see https://trello.com/c/kod5Ablj
}

void future_actor_dispatch(pony_actor_t* this, void* p, uint64_t id, int argc, pony_arg_t* argv) {
  init(this);

  switch (id) {
  case FUT_MSG_CHAIN:
    {
      // TODO: Add the closure argument to an internal list of closures
      // This entry should record the closure and the actor to run it
      fprintf(stderr, "Chaining on a future\n");
      Set chained = getChained(this);
      chained_entry *new_entry = pony_alloc(sizeof(chained_entry));
      new_entry->actor = argv[0].p;
      new_entry->closure = argv[1].p;
      set_add(chained, new_entry);
      break;
    }
  case FUT_MSG_BLOCK:
    {
      if (populated((future*) this)) {
	fprintf(stderr, "Reaching blocking but future is fulfilled\n");

	hacky_global_blocked_flag = false;

	blocked_entry new_entry = { .actor = argv[0].p , .context = argv[1].p };
	resume(&new_entry);
	
	break;
      }


      // TODO: Record the actor as one that needs to be woken up when the
      // future value is set
      fprintf(stderr, "Blocking on a future\n");
      Set blocked = getBlocked(this);
      blocked_entry *new_entry = pony_alloc(sizeof(blocked_entry));
      new_entry->actor = argv[0].p;
      new_entry->context = argv[1].p;
      set_add(blocked, new_entry);
      break;
    }
  case FUT_MSG_YIELD:
    {
      // TODO: Record the actor as one that should be sent the resume
      // message (2nd argument) when the future value is set
      // the yielded set can be merged with the blocked set
      fprintf(stderr, "Yielding on a future\n");
      Set yielded = getYielded(this);
      blocked_entry *new_entry = pony_alloc(sizeof(blocked_entry));
      new_entry->actor = argv[0].p;
      new_entry->context = argv[1].p;
      set_add(yielded, new_entry);
      break;
    }
  case FUT_MSG_FULFIL:
    {
      // TODO:
      // - put all blocking actors back into the scheduler queue
      // - send the appropriate resume messages to all yielding actors
      fprintf(stderr, "Fulfilling on a future\n");

      hacky_global_blocked_flag = false;

      Set chained = getChained(this);
      set_forall(chained, (void *((*)(void *))) run_chain);

      Set yielded = getYielded(this);
      set_forall(yielded, (void *((*)(void *))) resume);

      Set blocked = getBlocked(this);
      set_forall(blocked, (void *((*)(void *))) resume);

      // For unblocking, see pony's scheduler.c line 70
      break;
    }
  case FUT_MSG_RESUME:
    {
      fprintf(stderr, "Resuming because we got a resume message from a future\n");
      setResuming(true);
      setcontext(argv[0].p);
      break;
    }
  case FUT_MSG_RUN_CLOSURE:
    {
      void *closure = argv[0].p;
      // XXX do what's necessary to run the closure obj
      // XXX this should probably trigger an error in a future object! 
      break;
    }
  }
}