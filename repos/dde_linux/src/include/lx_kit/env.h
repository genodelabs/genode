/*
 * \brief  Globally available Lx_kit environment, needed in the C-ish lx_emul
 * \author Stefan Kalkowski
 * \date   2021-03-17
 */

/*
 * Copyright (C) 2021 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

#ifndef _LX_KIT__ENV_H_
#define _LX_KIT__ENV_H_

#include <base/env.h>
#include <timer_session/connection.h>
#include <lx_kit/console.h>
#include <lx_kit/device.h>
#include <lx_kit/init.h>
#include <lx_kit/memory.h>
#include <lx_kit/scheduler.h>
#include <lx_kit/timeout.h>

namespace Lx_kit {

	struct Env;

	/**
	 * Returns the global Env object available
	 *
	 * \param env - pointer to Genode::Env used to construct object initially
	 */
	Env & env(Genode::Env * env = nullptr);
}


struct Lx_kit::Env
{
	Genode::Env        & env;
	Genode::Heap         heap            { env.ram(), env.rm() };
	Initcalls            initcalls       { heap                };
	Pci_fixup_calls      pci_fixup_calls { heap                };
	Console              console         { };
	Platform::Connection platform        { env };
	Timer::Connection    timer           { env };
	Mem_allocator        memory          { env, heap, platform, CACHED   };
	Mem_allocator        uncached_memory { env, heap, platform, UNCACHED };
	Scheduler            scheduler       { env.ep() };
	Device_list          devices         { env.ep(), heap, platform };
	Lx_kit::Timeout      timeout         { timer, scheduler };
	unsigned int         last_irq        { 0 };

	Env(Genode::Env & env) : env(env) { }
};

#endif /* _LX_KIT__ENV_H_ */
