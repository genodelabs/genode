pragma Ada_2012;

with LSC.AES_Generic.CBC;
with LSC.SHA2_Generic;
with Ada.Unchecked_Conversion;

package body Aes_Cbc_4k with SPARK_Mode
is
   function Init_IV (Key : Key_Type; Block_Number : Block_Number_Type)
      return Ciphertext_Base_Type
      with post => Init_IV'Result'Length = 16;

   function Init_IV (Key : Key_Type; Block_Number : Block_Number_Type)
      return Ciphertext_Base_Type
   is

      type IV_Key_Base_Type is array (Natural range <>) of Byte;
      subtype IV_Key_Index_Type is Natural range 1 .. 32;
      subtype IV_Key_Type is IV_Key_Base_Type (IV_Key_Index_Type);

      function Hash is new LSC.SHA2_Generic.Hash_SHA256
         (Natural, Byte, Key_Base_Type, IV_Key_Index_Type, Byte, IV_Key_Type);

      type Padding_Type is array (Natural range <>) of Byte;
      type Block_Number_Text_Type is record
         Block_Number : Block_Number_Type;
         Padding      : Padding_Type(1 .. 8);
      end record
      with Size => 128;

      Block_Number_Text : constant Block_Number_Text_Type :=
         (Block_Number => Block_Number, Padding => (others => 0));

      type Block_Number_Plaintext_Base_Type is array (Natural range <>) of Byte;
      subtype Block_Number_Plaintext_Index_Type is Natural range 1 .. 16;
      subtype Block_Number_Plaintext_Type is Block_Number_Plaintext_Base_Type (Block_Number_Plaintext_Index_Type);

      function Convert is new Ada.Unchecked_Conversion
         (Block_Number_Text_Type, Block_Number_Plaintext_Type);

      function Enc_Key is new LSC.AES_Generic.Enc_Key
         (Natural, Byte, IV_Key_Base_Type);

      function Encrypt is new LSC.AES_Generic.Encrypt
         (Natural, Byte, Block_Number_Plaintext_Base_Type, Natural, Byte, Ciphertext_Base_Type);

  begin
     return Encrypt (Plaintext => Convert(Block_Number_Text),
                     Key       => Enc_Key(Hash(Key), LSC.AES_Generic.L256)) (Natural'First .. Natural'First + 15);
   end Init_IV;

   procedure Encrypt (Key          :     Key_Type;
                      Block_Number :     Block_Number_Type;
                      Plaintext    :     Plaintext_Type;
                      Ciphertext   : out Ciphertext_Type)
   is
      function Enc_Key is new LSC.AES_Generic.Enc_Key
         (Natural, Byte, Key_Base_Type);

      procedure Encrypt is new LSC.AES_Generic.CBC.Encrypt
         (Natural, Byte, Plaintext_Base_Type, Natural, Byte, Ciphertext_Base_Type);

      IV : constant Ciphertext_Base_Type := Init_IV(Key, Block_Number);
   begin

      Encrypt (Plaintext  => Plaintext,
               IV         => IV,
               Key        => Enc_Key(Key, LSC.AES_Generic.L256),
               Ciphertext => Ciphertext);
   end Encrypt;


   procedure Decrypt (Key          :     Key_Type;
                      Block_Number :     Block_Number_Type;
                      Ciphertext   :     Ciphertext_Type;
                      Plaintext    : out Plaintext_Type)
   is
      function Dec_Key is new LSC.AES_Generic.Dec_Key
         (Natural, Byte, Key_Base_Type);

      procedure Decrypt is new LSC.AES_Generic.CBC.Decrypt
         (Natural, Byte, Plaintext_Base_Type, Natural, Byte, Ciphertext_Base_Type);

   begin
      Decrypt (Ciphertext => Ciphertext,
               IV         => Init_IV(Key, Block_Number),
               Key        => Dec_Key(Key, LSC.AES_Generic.L256),
               Plaintext  => Plaintext);
   end Decrypt;

end Aes_Cbc_4k;
