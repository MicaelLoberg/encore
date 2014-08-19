{-|
Defines how things will be called in the CCode generated by CodeGen.hs
Provides mappings from class/method names to their C-name.

The purpose of this module is to

 - get one central place where identifiers in the generated code can be changed

 - ease following of good conventions (ie use @Ptr char@ instead of @Embed "char*"@)

-}

module CodeGen.CCodeNames where

import qualified Identifiers as ID
import Types as Ty
import CCode.Main
import Data.Char

char = Typ "char"
int = Typ "int64_t"
uint = Typ "uint64_t"
bool = Typ "int64_t" -- For pony argument tag compatibility. Should be changed to something smaller
double = Typ "double"
void = Typ "void"
pony_actor_t = Typ "pony_actor_t"
pony_actor_type_t = Typ "pony_actor_type_t"
pony_arg_t = Typ "pony_arg_t"
pony_msg_t = Typ "pony_msg_t"
closure = Ptr $ Typ "closure_t"
future = Ptr $ Typ "future_t"
unit :: CCode Lval
unit = Embed "UNIT" 

-- | each method is implemented as a function with a `this`
-- pointer. This is the name of that function
method_impl_name :: Ty.Type -> ID.Name -> CCode Name
method_impl_name clazz mname =
    Nam $ (show clazz) ++ "_" ++ (show mname)

global_closure_name :: ID.Name -> CCode Name
global_closure_name funname =
    Nam $ (show funname)

global_function_name :: ID.Name -> CCode Name
global_function_name funname = 
    Nam $ "_global_" ++ (show funname) ++ "_fun"

closure_fun_name :: String -> CCode Name
closure_fun_name name =
    Nam $ "_" ++ name ++ "_fun"

closure_env_name :: String -> CCode Name
closure_env_name name =
    Nam $ "_" ++ name ++ "_env"

-- | each class, in C, provides a dispatch function that dispatches
-- messages to the right method calls. This is the name of that
-- function.
class_dispatch_name :: Ty.Type -> CCode Name
class_dispatch_name clazz = Nam $ (show clazz) ++ "_dispatch"

class_message_type_name :: Ty.Type -> CCode Name
class_message_type_name clazz = Nam $ (show clazz) ++ "_message_type"

class_trace_fn_name :: Ty.Type -> CCode Name
class_trace_fn_name clazz = Nam $ show clazz ++ "_trace"

method_message_type_name :: Ty.Type -> ID.Name -> CCode Lval --fixme should be a name
method_message_type_name clazz mname = Var $ "m_"++show clazz++"_"++show mname

one_way_message_type_name :: Ty.Type -> ID.Name -> CCode Lval --fixme should be a name
one_way_message_type_name clazz mname = Var $ "m_"++show clazz++"__one_way_"++show mname

-- | for each method, there's a corresponding message, this is its name
method_msg_name :: Ty.Type -> ID.Name -> CCode Name
method_msg_name clazz mname = Nam $ "MSG_"++show clazz++"_"++show mname

one_way_send_msg_name :: Ty.Type -> ID.Name -> CCode Name
one_way_send_msg_name clazz mname = Nam $ "MSG_"++show clazz++"__one_way_"++show mname

-- | the name of the record type in which a class stores its state
data_rec_name :: Ty.Type -> CCode Name
data_rec_name clazz = Nam $ show clazz ++ "_data"

data_rec_type :: Ty.Type -> CCode Ty
data_rec_type clazz = Typ $ show clazz ++ "_data"

data_rec_ptr :: Ty.Type -> CCode Ty
data_rec_ptr = Ptr . data_rec_type

actor_rec_name :: Ty.Type -> CCode Name
actor_rec_name clazz = Nam $ show clazz ++ "_actor"