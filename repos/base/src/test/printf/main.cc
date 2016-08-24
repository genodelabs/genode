/*
 * \brief  Testing 'printf()' with negative integer
 * \author Christian Prochaska
 * \date   2012-04-20
 *
 */

/*
 * Copyright (C) 2012-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#include <base/component.h>
#include <base/printf.h>
#include <base/log.h>


void Component::construct(Genode::Env &env)
{
	using namespace Genode;

	log("hex range:          ", Hex_range<uint16_t>(0xe00, 0x880));
	log("empty hex range:    ", Hex_range<uint32_t>(0xabc0000, 0));
	log("hex range to limit: ", Hex_range<uint8_t>(0xf8, 8));
	log("invalid hex range:  ", Hex_range<uint8_t>(0xf8, 0x10));
	log("negative hex char:  ", Hex((char)-2LL, Hex::PREFIX, Hex::PAD));
	log("positive hex char:  ", Hex((char) 2LL, Hex::PREFIX, Hex::PAD));

	/* test that unsupported commands don't crash the printf parser */
	printf("%#x %s\n", 0x38, "test 1");
	printf("%#lx %s\n", 0x38L, "test 2");
	printf("-1 = %d = %ld\n", -1, -1L);
}
