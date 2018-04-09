package body add_package is

procedure Add(A: in Integer;
              B: in Integer;
              R: out Integer)
is
	procedure ext_c_print_add(a, b : Integer; result : out Integer);
	pragma import(C, ext_c_print_add, "print_add");
begin
    ext_c_print_add(A, B, R);
    R := A + B;
end Add;

end add_package;
