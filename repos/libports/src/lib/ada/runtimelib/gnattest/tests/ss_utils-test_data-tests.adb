--  This package has been generated automatically by GNATtest.
--  You are allowed to add your code to the bodies of test routines.
--  Such changes will be kept during further regeneration of this file.
--  All code placed outside of test routine bodies will be lost. The
--  code intended to set up and tear down the test environment should be
--  placed into Ss_Utils.Test_Data.

with AUnit.Assertions; use AUnit.Assertions;
with System.Assertions;

--  begin read only
--  id:2.2/00/
--
--  This section can be used to add with clauses if necessary.
--
--  end read only
with System.Memory;
--  begin read only
--  end read only
package body Ss_Utils.Test_Data.Tests is

   --  begin read only
   --  id:2.2/01/
   --
   --  This section can be used to add global variables and other elements.
   --
   --  end read only

   package SSE renames System.Storage_Elements;
   Alloc_Success : Boolean := False;
   Valid_Thread : Thread := Thread (SSE.To_Address (16#1000#));

   function C_Alloc (T : Thread; Size : SSE.Storage_Count) return System.Address
     with
       Export,
       Convention => C,
       External_Name => "allocate_secondary_stack";

   function C_Alloc (T : Thread; Size : SSE.Storage_Count) return System.Address
   is
   begin
      if Alloc_Success then
         return System.Memory.Alloc (System.Memory.size_t (Size));
      else
         return System.Null_Address;
      end if;
   end C_Alloc;

   procedure Get_Mark_Full_Registry
   is
      E : Mark;
      R : Registry := Null_Registry;
      T : constant Thread :=  Thread (System.Storage_Elements.To_Address (42));
   begin
      for I in R'Range loop
         R (I).Id := Valid_Thread;
      end loop;
      Get_Mark (T, R, E);
   end Get_Mark_Full_Registry;

   procedure Get_Mark_Invalid_Thread
   is
      E : Mark;
      R : Registry := Null_Registry;
   begin
      Get_Mark (Invalid_Thread, R, E);
   end Get_Mark_Invalid_Thread;
   
   procedure Set_Mark_Unknown_Thread
   is
      T : Thread := Valid_Thread;
      R : Registry := Null_Registry;
      M : Mark := (Base  => System.Storage_Elements.To_Address (42),
                   Top   => 8);
   begin
      Set_Mark (T, M, R);
   end Set_Mark_Unknown_Thread;

   procedure Set_Mark_Invalid_Thread
   is
      T : Thread := Invalid_Thread;
      R : Registry := Null_Registry;
      M : Mark := (Base  => System.Storage_Elements.To_Address (42),
                            Top   => 8);
   begin
      Set_Mark (T, M, R);
   end Set_Mark_Invalid_Thread;

   procedure Set_Mark_Invalid_Stack
   is
      T : Thread := Valid_Thread;
      R : Registry := Null_Registry;
      M : Mark := (Base  => System.Null_Address,
                   Top   => 0);
   begin
      R (0).Id := T;
      R (0).Data := M;
      R (0).Data.Base := System.Storage_Elements.To_Address (42);
      Set_Mark (T, M, R);
   end Set_Mark_Invalid_Stack;
   
   procedure Alloc_Stack_With_Null_Ptr
   is
      Ptr : System.Address;
   begin
      Alloc_Success := False;
      Ptr := Allocate_Stack (Valid_Thread, 0);
   end Alloc_Stack_With_Null_Ptr;

   procedure Alloc_Stack_With_Invalid_Thread
   is
      Ptr : System.Address;
   begin
      Alloc_Success := True;
      Ptr := Allocate_Stack (Invalid_Thread, 0);
   end Alloc_Stack_With_Invalid_Thread;
   
   procedure S_Allocate_Stack_Overflow_1
   is
      T : Thread := Valid_Thread;
      Reg : Registry := Null_Registry;
      Stack_Ptr : System.Address;
   begin
      S_Allocate (Stack_Ptr, Secondary_Stack_Size * 2, Reg, T);
   end S_Allocate_Stack_Overflow_1;
   
   procedure S_Allocate_Stack_Overflow_2
   is
      T : Thread := Valid_Thread;
      Reg : Registry := Null_Registry;
      Stack_Ptr : System.Address;
   begin
      for I in 0 .. 1024 loop
         S_Allocate (Stack_Ptr, SSE.Storage_Offset (1024), Reg, T);
      end loop;
   end S_Allocate_Stack_Overflow_2;
   
   procedure S_Release_High_Mark
   is
      T : Thread := Valid_Thread;
      Reg : Registry := Null_Registry;
      Mark_Id : SSE.Storage_Count;
      Mark_Base : System.Address;
      Stack_Ptr : System.Address;
   begin
      S_Allocate (Stack_Ptr, 8, Reg, T);
      S_Mark (Mark_Base, Mark_Id, Reg, T);
      S_Release (Mark_Base, Mark_Id + 1, Reg, T);
   end S_Release_High_Mark;
   
   --  begin read only
   --  end read only

   --  begin read only
   procedure Test_Get_Mark (Gnattest_T : in out Test);
   procedure Test_Get_Mark_205a8b (Gnattest_T : in out Test) renames Test_Get_Mark;
   --  id:2.2/205a8b09655a71fc/Get_Mark/1/0/
   procedure Test_Get_Mark (Gnattest_T : in out Test) is
      --  ss_utils.ads:36:4:Get_Mark
      --  end read only

      pragma Unreferenced (Gnattest_T);
      T1 : Thread := Thread (System.Storage_Elements.To_Address (1));
      T2 : Thread := Thread (System.Storage_Elements.To_Address (2));
      Reg : Registry := Null_Registry;
      Test_Mark : Mark;
      T1_Mark : Mark := (Base => System.Null_Address,
                         Top   => 0);
      T2_Mark : Mark := (Base => System.Storage_Elements.To_Address (42),
                         Top   => 4);
   begin

      Alloc_Success := True;
      AUnit.Assertions.Assert_Exception (Get_Mark_Invalid_Thread'Access,
                                         "Get mark with invalid thread failed");
      AUnit.Assertions.Assert_Exception (Get_Mark_Full_Registry'Access,
                                         "Get mark with full registry failed");

      Get_Mark (T1, Reg, Test_Mark);
      AUnit.Assertions.Assert (Test_Mark.Base /= System.Null_Address,
                               "Stack not initialized");
      AUnit.Assertions.Assert (Test_Mark.Top = 0,
                               "Top not null after initialization");

      Get_Mark (T1, Reg, T1_Mark);
      AUnit.Assertions.Assert (Test_Mark = T1_Mark,
                               "Failed to get T1 mark");

      Reg (2).Id := T2;
      Reg (2).Data := T2_Mark;

      Get_Mark (T2, Reg, Test_Mark);
      AUnit.Assertions.Assert (Test_Mark = T2_Mark,
                               "Failed to get T2 mark");

      --  begin read only
   end Test_Get_Mark;
   --  end read only
   
   --  begin read only
   procedure Test_Set_Mark (Gnattest_T : in out Test);
   procedure Test_Set_Mark_75973c (Gnattest_T : in out Test) renames Test_Set_Mark;
   --  id:2.2/75973c43cd4409b1/Set_Mark/1/0/
   procedure Test_Set_Mark (Gnattest_T : in out Test) is
      --  ss_utils.ads:44:4:Set_Mark
      --  end read only

      pragma Unreferenced (Gnattest_T);
      Reg : Registry := Null_Registry;
      E1 : Mark;
      E2 : Mark;
      T : Thread := Valid_Thread;
   begin

      AUnit.Assertions.Assert_Exception (Set_Mark_Unknown_Thread'Access,
                                         "Set mark on unknown thread failed");
      AUnit.Assertions.Assert_Exception (Set_Mark_Invalid_Thread'Access,
                                         "Set mark on invalid thread failed");
      AUnit.Assertions.Assert_Exception (Set_Mark_Invalid_Stack'Access,
                                         "Set mark on invalid stack failed");

      Get_Mark (T, Reg, E1);
      E1.Top := 42;
      Set_Mark (T, E1, Reg);
      Get_Mark (T, Reg, E2);
      AUnit.Assertions.Assert (E1 = E2,
                               "Storing mark failed");
      --  begin read only
   end Test_Set_Mark;
   --  end read only


   --  begin read only
   procedure Test_Allocate_Stack (Gnattest_T : in out Test);
   procedure Test_Allocate_Stack_247b78 (Gnattest_T : in out Test) renames Test_Allocate_Stack;
   --  id:2.2/247b786dfb10deba/Allocate_Stack/1/0/
   procedure Test_Allocate_Stack (Gnattest_T : in out Test) is
      --  ss_utils.ads:53:4:Allocate_Stack
      --  end read only

      pragma Unreferenced (Gnattest_T);
   begin

      Alloc_Success := True;
      AUnit.Assertions.Assert (Allocate_Stack (Valid_Thread, 0) /= System.Null_Address,
                               "Allocate stack failed");
      AUnit.Assertions.Assert_Exception (Alloc_Stack_With_Null_Ptr'Access,
                                         "Allocate stack with null failed");
      AUnit.Assertions.Assert_Exception (Alloc_Stack_With_Invalid_Thread'Access,
                                         "Allocate stack with invalid thread failed");

      --  begin read only
   end Test_Allocate_Stack;
   --  end read only


   --  begin read only
   procedure Test_S_Allocate (Gnattest_T : in out Test);
   procedure Test_S_Allocate_70b783 (Gnattest_T : in out Test) renames Test_S_Allocate;
   --  id:2.2/70b783c9ffa2dd5e/S_Allocate/1/0/
   procedure Test_S_Allocate (Gnattest_T : in out Test) is
      --  ss_utils.ads:61:4:S_Allocate
      --  end read only

      pragma Unreferenced (Gnattest_T);
      Reg : Registry := Null_Registry;
      M : Mark;
      T : Thread := Valid_Thread;
      Stack_Base : System.Address;
      Stack_Ptr : System.Address;
   begin

      Alloc_Success := True;
      
      Get_Mark (T, Reg, M);
      AUnit.Assertions.Assert (M.Base /= System.Null_Address,
                               "Base allocation failed");
      AUnit.Assertions.Assert (M.Top = 0,
                               "Top not initialized with 0");
      
      Stack_Base := M.Base;
      S_Allocate (Stack_Ptr, 8, Reg, T);
      AUnit.Assertions.Assert (Stack_Base - 8 = Stack_Ptr,
                        "Invalid base move");
      Get_Mark (T, Reg, M);
      AUnit.Assertions.Assert (M.Top = 8,
                               "Unmodified top");
      
      S_Allocate (Stack_Ptr, 8, Reg, T);
      AUnit.Assertions.Assert (Stack_Base - 16 = Stack_Ptr,
                               "Invalid stack ptr");
      Get_Mark (T, Reg, M);
      AUnit.Assertions.Assert (M.Top = 16,
                               "Invalid top");
      
      Reg := Null_Registry;
      S_Allocate (Stack_Ptr, 8, Reg, T);
      Get_Mark (T, Reg, M);
      
      AUnit.Assertions.Assert (Stack_Ptr /= System.Null_Address,
                              "Initial Base allocation failed");
      AUnit.Assertions.Assert (M.Base - 8 = Stack_Ptr,
                               "Invalid Stack initialization");
      AUnit.Assertions.Assert (M.Top = 8,
                               "Top not set correctly");
      
      AUnit.Assertions.Assert_Exception (S_Allocate_Stack_Overflow_1'Access,
                                         "Failed to detect stack overflow 1");
      AUnit.Assertions.Assert_Exception (S_Allocate_Stack_Overflow_2'Access,
                                         "Failed to detect stack overflow 2");

      --  begin read only
   end Test_S_Allocate;
   --  end read only


   --  begin read only
   procedure Test_S_Mark (Gnattest_T : in out Test);
   procedure Test_S_Mark_a8299f (Gnattest_T : in out Test) renames Test_S_Mark;
   --  id:2.2/a8299fa36da74b68/S_Mark/1/0/
   procedure Test_S_Mark (Gnattest_T : in out Test) is
      --  ss_utils.ads:68:4:S_Mark
      --  end read only

      pragma Unreferenced (Gnattest_T);
      Reg : Registry := Null_Registry;
      M : Mark;
      S_Addr : System.Address;
      S_Pos : SSE.Storage_Count;
      T : Thread := Valid_Thread;
      Stack_Ptr : System.Address;
   begin

      S_Allocate (Stack_Ptr, 8, Reg, T);
      S_Mark (S_Addr, S_Pos, Reg, T);
      Get_Mark (T, Reg, M);
      AUnit.Assertions.Assert (S_Pos = M.Top and S_Pos = 8 and S_Addr = M.Base,
                               "Invalid mark location");
      
      Reg := Null_Registry;
      S_Mark (S_Addr, S_Pos, Reg, T);
      Get_Mark (T, Reg, M);
      AUnit.Assertions.Assert (S_Addr = M.Base,
                               "Mark did not initialize stack");
      AUnit.Assertions.Assert (M.Top = 0 and S_Pos = 0,
                               "Mark did not initialize top");

      --  begin read only
   end Test_S_Mark;
   --  end read only


   --  begin read only
   procedure Test_S_Release (Gnattest_T : in out Test);
   procedure Test_S_Release_666a46 (Gnattest_T : in out Test) renames Test_S_Release;
   --  id:2.2/666a463f3bb6be9f/S_Release/1/0/
   procedure Test_S_Release (Gnattest_T : in out Test) is
      --  ss_utils.ads:74:4:S_Release
      --  end read only

      pragma Unreferenced (Gnattest_T);
      Reg : Registry := Null_Registry;
      M : Mark;
      Stack_Ptr : System.Address;
      Mark_Id : System.Address;
      Mark_Pos : SSE.Storage_Count;
      T : Thread := Valid_Thread;
   begin
      
      AUnit.Assertions.Assert_Exception (S_Release_High_Mark'Access,
                                         "Invalid stack release");
      
      S_Allocate (Stack_Ptr, 8, Reg, T);
      S_Mark (Mark_Id, Mark_Pos, Reg, T);
      S_Allocate (Stack_Ptr, 4, Reg, T);
      S_Allocate (Stack_Ptr, 4, Reg, T);
      Get_Mark (T, Reg, M);
      
      AUnit.Assertions.Assert (M.Top = 16,
                               "Top not initialized correctly");
      AUnit.Assertions.Assert (Stack_Ptr /= Mark_Id - Mark_Pos,
                               "Mark not set correctly");
      
      S_Release (Mark_Id, Mark_Pos, Reg, T);
      Get_Mark (T, Reg, M);
      AUnit.Assertions.Assert (M.Top = 8,
                               "Top not reset correctly");
      AUnit.Assertions.Assert (M.Top = Mark_Pos,
                               "Invalid mark id");
      S_Allocate (Stack_Ptr, 8, Reg, T);
      AUnit.Assertions.Assert (Mark_Id - Mark_Pos = Stack_Ptr + 8,
                               "Invalid stack ptr location");
      
   --  begin read only
   end Test_S_Release;
   --  end read only


   --  begin read only
   procedure Test_C_Alloc (Gnattest_T : in out Test);
   procedure Test_C_Alloc_08db82 (Gnattest_T : in out Test) renames Test_C_Alloc;
   --  id:2.2/08db82b0ee8ae041/C_Alloc/1/0/
   procedure Test_C_Alloc (Gnattest_T : in out Test) is
      --  ss_utils.ads:80:4:C_Alloc
      --  end read only

      pragma Unreferenced (Gnattest_T);

   begin

      Alloc_Success := True;
      AUnit.Assertions.Assert (C_Alloc (Invalid_Thread, 0) /= System.Null_Address,
                               "Alloc test failed");
      Alloc_Success := False;
      AUnit.Assertions.Assert (C_Alloc (Invalid_Thread, 0) = System.Null_Address,
                               "Null Address test failed");

      --  begin read only
   end Test_C_Alloc;
   --  end read only


   --  begin read only
   procedure Test_C_Free (Gnattest_T : in out Test);
   procedure Test_C_Free_6747d0 (Gnattest_T : in out Test) renames Test_C_Free;
   --  id:2.2/6747d08d1141dd9a/C_Free/1/0/
   procedure Test_C_Free (Gnattest_T : in out Test) is
      --  ss_utils.ads:88:4:C_Free
      --  end read only

      pragma Unreferenced (Gnattest_T);

   begin

      AUnit.Assertions.Assert (True, "C_Free");

      --  begin read only
   end Test_C_Free;
   --  end read only


   --  begin read only
   procedure Test_C_Get_Thread (Gnattest_T : in out Test);
   procedure Test_C_Get_Thread_97edc5 (Gnattest_T : in out Test) renames Test_C_Get_Thread;
   --  id:2.2/97edc547916ca499/C_Get_Thread/1/0/
   procedure Test_C_Get_Thread (Gnattest_T : in out Test) is
      --  ss_utils.ads:94:4:C_Get_Thread
      --  end read only

      pragma Unreferenced (Gnattest_T);

   begin

      AUnit.Assertions.Assert (True, "C_Get_Thread");

      --  begin read only
   end Test_C_Get_Thread;
   --  end read only


   --  begin read only
   --  procedure Test_Read_Mark (Gnattest_T : in out Test_);
   --  procedure Test_Read_Mark_8404db (Gnattest_T : in out Test_) renames Test_Read_Mark;
   --  id:2.2/8404db30c2a22d7a/Read_Mark/1/1/
   --  procedure Test_Read_Mark (Gnattest_T : in out Test_) is
   --  end read only
--  begin read only
   --  end Test_Read_Mark;
   --  end read only
--  begin read only
   --  procedure Test_Write_Mark (Gnattest_T : in out Test_);
   --  procedure Test_Write_Mark_99a74f (Gnattest_T : in out Test_) renames Test_Write_Mark;
   --  id:2.2/99a74ffb3f48c3b7/Write_Mark/1/1/
   --  procedure Test_Write_Mark (Gnattest_T : in out Test_) is
   --  end read only
--  begin read only
   --  end Test_Write_Mark;
   --  end read only

   --  begin read only
   --  id:2.2/02/
   --
   --  This section can be used to add elaboration code for the global state.
   --
begin
   --  end read only
   null;
   --  begin read only
   --  end read only
end Ss_Utils.Test_Data.Tests;
