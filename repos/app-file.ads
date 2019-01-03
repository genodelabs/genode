pragma Ada_2012;

package App.File with Spark_Mode is
   
   type Chunk_Number_Type is new Integer;
   type Chunk_Type        is array (Positive range 1..4096) of Byte;

   procedure Read(Offset      : in     Offset_Type;
                  Destination : in out Chunk_Type;
                  Size        : in     Size_Type)
     with
       import,
       convention => c,
       external_name => "c_genode_file_read";
   
end App.File;
