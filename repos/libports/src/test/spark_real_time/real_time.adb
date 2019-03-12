with Ada.Real_Time;
with Ada.Command_Line;
with GNAT.IO;

procedure Real_Time
is
   use Ada.Real_Time;

   Start  : Time;
   Diff   : Time_Span;

   procedure Sleep (Seconds : Natural)
   with
      Import,
      Convention => C,
      External_Name => "sleep";
begin
   GNAT.IO.Put_Line ("Sleeping for 5 seconds.");
   Start := Clock;
   Sleep (5);
   Diff := Clock - Start;

   if Diff > Microseconds (5050000)
   then
      GNAT.IO.Put_Line ("Error: Slept too long");
      Ada.Command_Line.Set_Exit_Status (1);
   elsif Diff < Microseconds (4950000)
   then
      GNAT.IO.Put_Line ("Error: Slept too short");
      Ada.Command_Line.Set_Exit_Status (2);
   else
      GNAT.IO.Put_Line ("Slept OK");
      Ada.Command_Line.Set_Exit_Status (0);
   end if;
end Real_Time;
