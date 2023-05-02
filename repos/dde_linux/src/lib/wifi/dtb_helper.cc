/**
 * \brief  DTB access helper
 * \author Josef Soentgen
 * \date   2023-04-11
 */

/*
 * Copyright (C) 2023 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

/* Genode includes */
#include <base/attached_rom_dataspace.h>
#include <util/reconstructible.h>

/* local includes */
#include "dtb_helper.h"

using namespace Genode;


struct Dtb
{
	Genode::Env &_env;

	Attached_rom_dataspace  _config_rom { _env, "config" };

	using Dtb_name = Genode::String<64>;

	Dtb_name _dtb_name {
		_config_rom.xml().attribute_value("dtb", Dtb_name("dtb")) };

	Attached_rom_dataspace _dtb_rom { _env, _dtb_name.string() };

	Dtb(Genode::Env &env) : _env { env } { }

	void *ptr()
	{
		return _dtb_rom.local_addr<void>();
	}
};


static Constructible<Dtb> _dtb { };


Dtb_helper::Dtb_helper(Genode::Env &env) : _env { env }
{
	try {
		_dtb.construct(env);
	} catch (...) {
		error("could not access DTB ROM module, driver may not work"
		      " as expected");
	}
}


void *Dtb_helper::dtb_ptr()
{
	return _dtb.constructed() ? _dtb->ptr()
	                          : nullptr;
}
