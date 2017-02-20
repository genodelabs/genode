/*
 * \brief  Helper class to make the Genode Env globally available
 * \author Sebastian Sumpf
 * \date   2016-06-21
 */

/*
 * Copyright (C) 2016-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__RUMP__ENV_H_
#define _INCLUDE__RUMP__ENV_H_

#include <base/attached_rom_dataspace.h>
#include <base/env.h>
#include <base/heap.h>
#include <util/reconstructible.h>

namespace Rump {
	class Env;

	Env &env();

	void construct_env(Genode::Env &env);
}

class Rump::Env
{
	private:

		Genode::Env                   &_env;
		Genode::Heap                   _heap { _env.ram(), _env.rm() };
		Genode::Attached_rom_dataspace _config { _env, "config" };

	public:

		Env(Genode::Env &env) : _env(env) { }

		Genode::Env                    &env()        { return _env; }
		Genode::Heap                   &heap()       { return _heap; }
		Genode::Attached_rom_dataspace &config_rom() { return _config; }
};

#endif /* _INCLUDE__RUMP__ENV_H_ */
