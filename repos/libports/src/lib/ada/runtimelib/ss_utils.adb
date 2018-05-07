package body Ss_Utils
  with SPARK_Mode
is

   procedure Get_Mark (
                       T               : Thread;
                       Thread_Registry : in out Registry;
                       E               : out Mark
                      )
   is
      Thread_Entry     : Long_Integer := -1;
      First_Free_Entry : Long_Integer := -1;
   begin
      Search :
      for E in Thread_Registry'Range loop
         pragma Loop_Invariant (First_Free_Entry < 128);
         pragma Loop_Invariant (Thread_Entry < 128);
         if First_Free_Entry = -1 and then
           Thread_Registry (E).Id = Invalid_Thread
         then
            First_Free_Entry := E;
         end if;

         if T = Thread_Registry (E).Id then
            Thread_Entry := E;
            exit Search;
         end if;
      end loop Search;

      if Thread_Entry < 0 then
         if First_Free_Entry >= 0 then
            Thread_Registry (First_Free_Entry).Id := T;
            Thread_Registry (First_Free_Entry).Data :=
              (Base  => System.Null_Address,
               Top   => 0);
            Thread_Entry := First_Free_Entry;
         else
            raise Constraint_Error;
         end if;
      end if;

      if Thread_Registry (Thread_Entry).Data.Base = System.Null_Address then
         Thread_Registry (Thread_Entry).Data :=
           (Base  => Allocate_Stack (T, Secondary_Stack_Size),
            Top   => 0);
      end if;

      E := Thread_Registry (Thread_Entry).Data;
   end Get_Mark;

   procedure Set_Mark (
                       T               : Thread;
                       M               : Mark;
                       Thread_Registry : in out Registry
                      )
   is
      Thread_Entry : Long_Integer := -1;
   begin
      if T = Invalid_Thread then
         raise Constraint_Error;
      end if;
      if M.Base = System.Null_Address then
         raise Constraint_Error;
      end if;

      Search :
      for E in Thread_Registry'Range loop
         pragma Loop_Invariant (Thread_Entry < 128);
         if T = Thread_Registry (E).Id then
            Thread_Entry := E;
            exit Search;
         end if;
      end loop Search;

      if Thread_Entry < 0 then
         raise Constraint_Error;
      end if;

      Thread_Registry (Thread_Entry).Data := M;
   end Set_Mark;

   function Allocate_Stack (
                            T    : Thread;
                            Size : SSE.Storage_Count
                           ) return System.Address
   is
      Stack : System.Address;
   begin
      if T = Invalid_Thread then
         raise Constraint_Error;
      end if;
      Stack := C_Alloc (T, Size);
      if Stack = System.Null_Address then
         raise Storage_Error;
      end if;
      return Stack;
   end Allocate_Stack;

   procedure S_Allocate (
                         Address      : out System.Address;
                         Storage_Size : SSE.Storage_Count;
                         Reg          : in out Registry;
                         T            : Thread
                        )
   is
      M         : Mark;
   begin
      Get_Mark (T, Reg, M);
      if M.Top < Secondary_Stack_Size and then
        Storage_Size < Secondary_Stack_Size and then
        Storage_Size + M.Top < Secondary_Stack_Size
      then
         M.Top := M.Top + Storage_Size;
         Address := M.Base - M.Top;
      else
         raise Storage_Error;
      end if;

      Set_Mark (T, M, Reg);
   end S_Allocate;

   procedure S_Mark (
                     Stack_Base : out System.Address;
                     Stack_Ptr  : out SSE.Storage_Count;
                     Reg        : in out Registry;
                     T          : Thread
                    )
   is
      M : Mark;
   begin
      Get_Mark (T, Reg, M);

      Stack_Base := M.Base;
      Stack_Ptr := M.Top;
   end S_Mark;

   procedure S_Release (
                        Stack_Base : System.Address;
                        Stack_Ptr  : SSE.Storage_Count;
                        Reg        : in out Registry;
                        T          : Thread
                       )
   is
      LM : Mark;
   begin
      Get_Mark (T, Reg, LM);

      if Stack_Ptr > LM.Top or Stack_Base /= LM.Base
      then
         raise Program_Error;
      end if;

      LM.Top := Stack_Ptr;

      Set_Mark (T, LM, Reg);
   end S_Release;

end Ss_Utils;
