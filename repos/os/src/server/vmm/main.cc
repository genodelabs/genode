/*
 * \brief  VMM example for ARMv8 virtualization
 * \author Stefan Kalkowski
 * \date   2014-07-08
 */

/*
 * Copyright (C) 2014-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <config.h>
#include <vm.h>
#include <base/attached_rom_dataspace.h>
#include <base/component.h>
#include <base/env.h>
#include <base/heap.h>
#include <util/reconstructible.h>

using namespace Genode;


struct Main
{
	Env                  & env;
	Heap                   heap       { env.ram(), env.rm()            };
	Attached_rom_dataspace config_rom { env, "config"                  };
	Signal_handler<Main>   handler    { env.ep(), *this, &Main::update };
	Vmm::Config            config     { heap                           };
	Constructible<Vmm::Vm> vm {};

	void update()
	{
		config_rom.update();
		config.update(config_rom.xml());
		vm.construct(env, heap, config);
	}

	Main(Env & env) : env(env)
	{
		config_rom.sigh(handler);
		update();
	}

};


void Component::construct(Env & env) { static Main m(env); }
