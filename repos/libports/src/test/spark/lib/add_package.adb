package body add_package is

procedure Add (A : in Integer;
               B : in Integer;
               R : out Integer)
is
	procedure Ext_C_Print_Add (A, B : Integer; Result : out Integer);
	pragma Import (C, Ext_C_Print_Add, "print_add");
begin
    Ext_C_Print_Add (A, B, R);
    R := R + 1;
end Add;

end add_package;
