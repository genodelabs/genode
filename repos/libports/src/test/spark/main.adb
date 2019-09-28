--
--  \brief  Ada test program
--  \author Norman Feske
--  \date   2009-09-23
--

with Add_Package;

--
--  Main program
--
procedure main is

   result : Integer;

   --
   --  Declarations of external C functions
   --
   procedure ext_c_print_int (a : Integer);
   pragma Import (C, ext_c_print_int, "print_int");

begin
   Add_Package.Add (13, 14, result);
   ext_c_print_int (result);
end main;
