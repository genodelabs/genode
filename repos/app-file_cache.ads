pragma Ada_2012;

with App.File;


package App.File_Cache with Spark_Mode is
   
   type Slot_Array_Index_Type  is new Positive range 1..128;
   type Slot_Array_Public_Type is array (Slot_Array_Index_Type) of File.Chunk_Type;
   
   type Object_Private_Type is private;
   type Object_Public_Type  is record
      Slot_Array : Slot_Array_Public_Type;
   end record;
   
   procedure Read_Chunk(Object_Private : in out Object_Private_Type;
                        Object_Public  : in out Object_Public_Type;
                        Chunk_Number   : in     File.Chunk_Number_Type;
                        Chunk_In_Slot  : out    Boolean;
                        Slot_Index     : out    Slot_Array_Index_Type);
   
private
   
   type Slot_Type is record
      Used         : Boolean           := False;
      Chunk_Number : File.Chunk_Number_Type := 0;
   end record;
   
   type Slot_Array_Private_Type is array (Slot_Array_Index_Type) of Slot_Type;
   type Object_Private_Type is record
      Slot_Array_Last_Index : Slot_Array_Index_Type;
      Slot_Array            : Slot_Array_Private_Type;
   end record;
   
end App.File_Cache;
