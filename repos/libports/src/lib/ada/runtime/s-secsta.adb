package body System.Secondary_Stack is

   procedure SS_Allocate (
                          Address      : out System.Address;
                          Storage_Size : SSE.Storage_Count
                         )
   is
      T : constant Ss_Utils.Thread := Ss_Utils.C_Get_Thread;
   begin
      if T /= Ss_Utils.Invalid_Thread then
         Ss_Utils.S_Allocate (Address,
                              Storage_Size,
                              Thread_Registry,
                              T);
      else
         raise Program_Error;
      end if;
   end SS_Allocate;

   function SS_Mark return Mark_Id
   is
      M : Mark_Id;
      T : constant Ss_Utils.Thread := Ss_Utils.C_Get_Thread;
   begin
      if T /= Ss_Utils.Invalid_Thread then
         Ss_Utils.S_Mark (M.Sstk,
                          SSE.Storage_Count (M.Sptr),
                          Thread_Registry,
                          T);
      else
         raise Program_Error;
      end if;
      return M;
   end SS_Mark;

   procedure SS_Release (
                         M : Mark_Id
                        )
   is
      T : constant Ss_Utils.Thread := Ss_Utils.C_Get_Thread;
   begin
      if T /= Ss_Utils.Invalid_Thread then
         Ss_Utils.S_Release (M.Sstk,
                             SSE.Storage_Count (M.Sptr),
                             Thread_Registry,
                             T);
      else
         raise Program_Error;
      end if;
   end SS_Release;

end System.Secondary_Stack;
