--
-- \brief  Ada test program that calls a external C functions
-- \author Norman Feske
-- \date   2009-09-23
--

with test_package;

--
-- Main program
--
procedure main is

	result : Integer;

	r : test_package.some_range_t;

	--
	-- Declarations of external C functions
	--
	procedure ext_c_add(a, b : Integer; result : out Integer);
	pragma import(C, ext_c_add, "add");

	procedure ext_c_print_int(a : Integer);
	pragma import(C, ext_c_print_int, "print_int");

begin
	ext_c_add(13, 14, result);
	ext_c_print_int(result);
end main;
