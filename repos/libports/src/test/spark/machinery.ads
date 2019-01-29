package Machinery is

   pragma Pure;

   type Temperature_Type is mod 2**32 with Size => 32;

   type Machinery_Type is private;

   procedure Initialize (Machinery : out Machinery_Type)
     with Export,
          Convention    => C,
          External_Name => "_ZN5Spark9MachineryC1Ev";

   function Temperature (Machinery : Machinery_Type) return Temperature_Type
     with Export,
          Convention    => C,
          External_Name => "_ZNK5Spark9Machinery11temperatureEv";

   procedure Heat_up (Machinery : in out Machinery_Type)
     with Export,
          Convention    => C,
          External_Name => "_ZN5Spark9Machinery7heat_upEv";

private

   type Machinery_Type is record
      Temperature : Temperature_Type;
   end record with Size => 32;

end Machinery;
