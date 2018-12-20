package Aes_Cbc_4k with SPARK_Mode
is

   -- pragma Pure; -- not possible because libsparkcrypto is not known as pure

   type Byte              is mod 2**8 with Size => 8;
   type Key_Base_type     is array (Natural range <>) of Byte;
   subtype Key_Type       is Key_Base_type (1 .. 32);
   type Block_Number_Type is mod 2**64 with Size => 64;

   type Plaintext_Base_Type is array (Natural range <>) of Byte;
   subtype Plaintext_Index_Type is Natural range 1 .. 4096;
   subtype Plaintext_Type is Plaintext_Base_Type (Plaintext_Index_Type);

   type Ciphertext_Base_Type is array (Natural range <>) of Byte;
   subtype Ciphertext_Index_Type is Natural range 1 .. 4096;
   subtype Ciphertext_Type is Ciphertext_Base_Type (Ciphertext_Index_Type);

   procedure Encrypt (Key          :     Key_Type;
                      Block_Number :     Block_Number_Type;
                      Plaintext    :     Plaintext_Type;
                      Ciphertext   : out Ciphertext_Type)
   with Export,
      Convention    => C,
      External_Name => "_ZN10Aes_cbc_4k7encryptERKNS_3KeyENS_12Block_numberERKNS_9PlaintextERNS_10CiphertextE";

   procedure Decrypt (Key          :     Key_Type;
                      Block_Number :     Block_Number_Type;
                      Ciphertext   :     Ciphertext_Type;
                      Plaintext    : out Plaintext_Type)
   with Export,
      Convention    => C,
      External_Name => "_ZN10Aes_cbc_4k7decryptERKNS_3KeyENS_12Block_numberERKNS_10CiphertextERNS_9PlaintextE";

end Aes_Cbc_4k;
