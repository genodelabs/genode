/*
 * \brief  Black hole component
 * \author Christian Prochaska
 * \date   2021-07-07
 *
 */

/*
 * Copyright (C) 2021 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */


/* Genode includes */
#include <base/attached_rom_dataspace.h>
#include <base/component.h>
#include <base/heap.h>
#include <base/log.h>
#include <root/component.h>

/* local includes */
#include "audio_out.h"


/***************
 ** Component **
 ***************/

namespace Black_hole { struct Main; }

struct Black_hole::Main
{
	Genode::Env &env;

	Genode::Sliced_heap heap { env.ram(), env.rm() };

	Genode::Constructible<Audio_out::Root> audio_out_root { };

	Main(Genode::Env &env) : env(env)
	{
		Genode::Attached_rom_dataspace _config_rom { env, "config" };

		if (_config_rom.xml().has_sub_node("audio_out")) {
			audio_out_root.construct(env, heap);
			env.parent().announce(env.ep().manage(*audio_out_root));
		}
	}
};


void Component::construct(Genode::Env &env)
{ static Black_hole::Main inst(env); }
