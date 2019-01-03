pragma Ada_2012;

with App.File;

use type App.File.Chunk_Number_Type;

package body App.File_Cache with Spark_Mode is

   procedure Read_Chunk(Object_Private : in out Object_Private_Type;
                        Object_Public  : in out Object_Public_Type;
                        Chunk_Number   : in     File.Chunk_Number_Type;
                        Chunk_In_Slot  : out    Boolean;
                        Slot_Index     : out    Slot_Array_Index_Type)
   is
--      Object_Public.Slot_Array(1)'Address
   begin
      
      Object_Private.Slot_Array_Last_Index := 1;
      Object_Public.Slot_Array(1)(1) := 0; 
      Chunk_In_Slot := False;
      Slot_Index := 1;
      
      Slot_Array_Loop : for Index in Object_Private.Slot_Array'Range loop
         if Object_Private.Slot_Array(Index).Chunk_Number = Chunk_Number then
            Chunk_In_Slot := True;
            Slot_Index := Index;
            exit Slot_Array_Loop;
         end if;
      end loop Slot_Array_Loop;
      
      if not Chunk_In_Slot then
          Log("Chunk in Slot");
--         File.Read(0, , Chunk_Type'Size / Byte'Size);
      else  
         Log("Chunk not in Slot");
      end if;
        
   end Read_Chunk;

end App.File_Cache;
