--
-- \brief  Ada test program that calls a external C functions
-- \author Norman Feske
-- \date   2009-09-23
--

with add_package;

--
-- Main program
--
procedure main is

	result : Integer;

	--
	-- Declarations of external C functions
	--
	procedure ext_c_print_int(a : Integer);
	pragma import(C, ext_c_print_int, "print_int");

begin
        add_package.Add(13, 14, result);
	ext_c_print_int(result);
end main;
