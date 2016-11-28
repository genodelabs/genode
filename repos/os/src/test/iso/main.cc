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

#include <base/component.h>
#include <base/log.h>
#include <os/attached_rom_dataspace.h>

using namespace Genode;


struct Hexdump
{
	uint8_t const * const ptr;
	unsigned long const offset;

	Hexdump(uint8_t const *ptr, unsigned long offset)
	: ptr(ptr), offset(offset) { }

	void print(Genode::Output &out) const
	{
		using Genode::print;
		unsigned long off = offset;
		unsigned long *_ptr = (unsigned long *)(ptr + off);

		for (int i = 0; i < 4; i++) {
			print(out, Hex((uint32_t)off, Hex::OMIT_PREFIX, Hex::PAD), ": ");

			int j;
			for (j = 0; j < 5; j++)
				print(out, Hex(_ptr[(i * 8) + j], Hex::OMIT_PREFIX, Hex::PAD), "  ");

			off += j * sizeof(unsigned long);
			print(out, "\n");
		}

		print(out, "\n");
	}
};


void Component::construct(Genode::Env &env)
{
	Attached_rom_dataspace ds(env, "/test.txt");

	uint8_t * const ptr = ds.local_addr<uint8_t>();

	log("File size is ", Hex(ds.size(), Hex::OMIT_PREFIX), " "
	    "at ", Hex((addr_t)ptr, Hex::OMIT_PREFIX));

	log(Hexdump(ptr, 0x1000));
	log(Hexdump(ptr, 0x10000));
	log(Hexdump(ptr, 0x20000));

	Attached_rom_dataspace rom("/notavail.txt");
	if (!rom.valid())
		log("Expected ROM error occured");
	else
		error("found file where no file should be!");

	env.parent().exit(0);
}
