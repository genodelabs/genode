/*
 * \brief  Simple Iso9660 test program
 * \author Sebastian Sumpf
 * \date   2010-07-26
 */

/*
 * Copyright (C) 2010-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#include <base/printf.h>
#include <os/attached_rom_dataspace.h>

using namespace Genode;


void dump(uint8_t *ptr, unsigned long offset)
{
	unsigned long *_ptr = (unsigned long *)(ptr + offset);
	for (int i = 0; i < 4; i++) {
		printf("%08lx: ", offset);

		int j;
		for (j = 0; j < 5; j++)
			printf("%08lx  ", _ptr[(i * 8) + j]);

		offset += j * sizeof(unsigned long);
		printf("\n");
	}

	printf("\n");
}


int main()
{
	Attached_rom_dataspace *ds;
	uint8_t *ptr = 0;

	try {

		ds  = new (env()->heap())Attached_rom_dataspace("/test.txt");
		ptr = ds->local_addr<uint8_t>();
		printf("File size is %zx at %p\n", ds->size(), ptr);
	}
	catch (...) {
		PDBG("Rom error");
		return 1;
	}

	dump(ptr, 0x1000);
	dump(ptr, 0x10000);
	dump(ptr, 0x20000);

	Attached_rom_dataspace rom("/notavail.txt");
	if (!rom.valid())
		PDBG("Expected ROM error occured");
	else
		PERR("found file where no file should be!");

	return 0;
}
