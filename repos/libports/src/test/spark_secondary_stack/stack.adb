with System;

package body Stack
with SPARK_Mode
is

    pragma Suppress (All_Checks);

    ------------
    -- Calloc --
    ------------

    procedure Calloc
      (Size : Integer)
      with SPARK_Mode => Off
    is
        procedure Print (Str : System.Address)
          with
            Import,
            Convention => C,
            External_Name => "print_stack";

        Buf : Buffer := Alloc (Size) & Character'Val (0);
    begin
        Print (Buf'Address);
    end Calloc;

    procedure Ralloc
    is
        R : constant Integer := 0;
        B : Buffer := Recursive_Alloc (R);
    begin
        null;
    end Ralloc;

    -----------
    -- Alloc --
    -----------

    function Alloc
      (Size : Integer)
       return Buffer
    is
        Buf : constant Buffer (1 .. Size) := (others => '0');
    begin
        return Buf;
    end Alloc;

    function Recursive_Alloc (Round : Integer) return Buffer
    is
        procedure Print (R : Integer)
          with
            Import,
            Convention => C,
            External_Name => "print_recursion";

        Buf : constant Buffer (1 .. 256) := (others => '0');
    begin
        Print (Round);
        if Round < 10 then
            return Buf & Recursive_Alloc (Round + 1);
        else
            return Buf;
        end if;
    end Recursive_Alloc;

    procedure Salloc
    is
        S : constant Integer := 16;
        B : Buffer := Stage_1 (S);
    begin
        Print_Stage (0);
    end Salloc;

    function Stage_1 (
                      Size : Integer
                     ) return Buffer
    is
        Buf : Buffer (1 .. Size) := (others => '0');
    begin
        Print_Stage (1);
        declare
            Buf_2 : constant Buffer := Stage_2 (Size);
        begin
            Buf := Buf_2;
        end;
        return Buf;
    end Stage_1;


    function Stage_2 (
                      Size : Integer
                     ) return Buffer
    is
        Buf : Buffer (1 .. Size);
    begin
        Print_Stage (2);
        declare
            Buf2 : constant Buffer := Stage_3 (Size);
        begin
            Buf := Buf2;
        end;
        return Buf;
    end Stage_2;

    function Stage_3 (
                      Size : Integer
                     ) return Buffer
    is
        Buf : Buffer (1 .. Size) := (others => '3');
    begin
        Print_Stage (3);
        return Buf;
    end Stage_3;

end Stack;
