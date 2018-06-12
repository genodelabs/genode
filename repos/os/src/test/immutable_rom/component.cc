/*
 * \brief  Test of read-only ROM services
 * \author Emery Hemingway
 * \date   2018-05-15
 */

/*
 * Copyright (C) 2018 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <base/attached_rom_dataspace.h>
#include <base/component.h>

void Component::construct(Genode::Env &env)
{
	using namespace Genode;

	Attached_rom_dataspace rom(env, "test");
	log("--- writing to ROM dataspace ---");
	for (size_t i = 0; i < rom.size(); ++i) {
		rom.local_addr<char>()[i] = i;
		log("--- ROM dataspace modified at ", (Hex)i, "! ---");
	}
	env.parent().exit(0);
}
