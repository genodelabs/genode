package Stack
with SPARK_Mode
is

    type Buffer is array (Integer range <>) of Character;

    procedure Calloc (
                      Size : Integer
                     );

    procedure Ralloc;

    function Alloc (
                    Size : Integer
                   ) return Buffer;

    function Recursive_Alloc (
                              Round : Integer
                             ) return Buffer;

    procedure Salloc;

    function Stage_1 (
                      Size : Integer
                     ) return Buffer;

    function Stage_2 (
                      Size : Integer
                     ) return Buffer;

    function Stage_3 (
                      Size : Integer
                     ) return Buffer;

private

    procedure Print_Stage (
                           Stage : Integer
                          )
      with
        Import,
        Convention => C,
        External_Name => "print_stage";

end Stack;
