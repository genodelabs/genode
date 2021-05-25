/*
 * \brief  Platform driver for ARM
 * \author Stefan Kalkowski
 * \date   2020-04-12
 */

/*
 * Copyright (C) 2020 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _SRC__DRIVERS__PLATFORM__SPEC__ARM__ENV_H_
#define _SRC__DRIVERS__PLATFORM__SPEC__ARM__ENV_H_

#include <base/attached_rom_dataspace.h>
#include <base/env.h>
#include <base/heap.h>

#include <ccm.h>
#include <gpc.h>
#include <src.h>
#include <device.h>

namespace Driver {
	using namespace Genode;

	struct Env;
};


struct Driver::Env
{
	Genode::Env          & env;
	Heap                   heap        { env.ram(), env.rm() };
	Sliced_heap            sliced_heap { env.ram(), env.rm() };
	Attached_rom_dataspace config      { env, "config"       };
	Ccm                    ccm         { env                 };
	Gpc                    gpc         { env                 };
	Src                    src         { env                 };
	Device_model           devices     { *this               };

	Env(Genode::Env &env) : env(env) { }
};

#endif /* _SRC__DRIVERS__PLATFORM__SPEC__ARM__ENV_H_ */
