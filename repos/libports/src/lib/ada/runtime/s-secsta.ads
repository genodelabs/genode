with System.Storage_Elements;
with Ss_Utils;
use all type Ss_Utils.Thread;

package System.Secondary_Stack is

   package SSE renames System.Storage_Elements;

   type Mark_Id is private;

   procedure SS_Allocate (
                          Address      : out System.Address;
                          Storage_Size : SSE.Storage_Count
                         );

   function SS_Mark return Mark_Id;

   procedure SS_Release (
                         M : Mark_Id
                        );

private

   type Mark_Id is record
      Sstk : System.Address;
      Sptr : SSE.Integer_Address;
   end record;

   Thread_Registry : Ss_Utils.Registry := Ss_Utils.Null_Registry;

end System.Secondary_Stack;
