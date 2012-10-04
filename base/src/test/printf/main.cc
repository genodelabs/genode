/*
 * \brief  Testing 'printf()' with negative integer
 * \author Christian Prochaska
 * \date   2012-04-20
 *
 */

/*
 * Copyright (C) 2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#include <base/printf.h>

int main(int argc, char **argv)
{
	Genode::printf("%%d    - '-1'       = '%d'       = '%ld'\n", -1, -1L);
	Genode::printf("%%8d   - '      -1' = '%8d' = '%8ld'\n", -1, -1L);
	Genode::printf("%%08d  - '-0000001' = '%08d' = '%08ld'\n", -1, -1L);
	Genode::printf("%%-8d  - '-1      ' = '%-8d' = '%-8ld'\n", -1, -1L);

	Genode::printf("%%d    - '1'        = '%d'        = '%ld'\n", 1, 1L);
	Genode::printf("%%8d   - '       1' = '%8d' = '%8ld'\n", 1, 1L);
	Genode::printf("%%08d  - '00000001' = '%08d' = '%08ld'\n", 1, 1L);
	Genode::printf("%%-8d  - '1       ' = '%-8d' = '%-8ld'\n", 1, 1L);

	Genode::printf("%%+d   - '+1'       = '%+d'       = '%+ld'\n", 1, 1L);
	Genode::printf("%%+8d  - '      +1' = '%+8d' = '%+8ld'\n", 1, 1L);
	Genode::printf("%%+08d - '+0000001' = '%+08d' = '%+08ld'\n", 1, 1L);
	Genode::printf("%%-+8d - '+1      ' = '%-+8d' = '%-+8ld'\n", 1, 1L);


	Genode::printf("%%8u   - '       1' = '%8u' = '%8lu'\n", 1U, 1UL);
	Genode::printf("%%08u  - '00000001' = '%08u' = '%08lu'\n", 1U, 1UL);
	Genode::printf("%%-8u  - '1       ' = '%-8u' = '%-8lu'\n", 1U, 1UL);

	Genode::printf("'%%p'      := '%p'; '%%8p' := '%8p'\n", argv, argv);

	return 0;
}
