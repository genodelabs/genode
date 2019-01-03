--
-- \brief  GNAT.IO test program
-- \author Alexander Senier
-- \date   2019-01-03
--

with GNAT.IO;

procedure Main is
   use GNAT.IO;
begin
   Put (Standard_Output, "Hell");
   Put (Standard_Output, 'o');
   Put_Line (Standard_Output, " World!");
   New_Line (Standard_Output);
   Put (Standard_Output, 98765432);
   New_Line;

   Put ("Integer: ");
   Put (123456);
   New_Line;

   Put_Line ("Character: ");
   Put ('X');
   Put ('Y');
   Put ('Z');
   New_Line;

   Put ("New_Line with spacing:");
   New_Line (Spacing => 5);

   Set_Output (Standard_Error);
   Put (11223344);
   New_Line;
   Put_Line ("GNAT.IO test program started successfully.");
end Main;
