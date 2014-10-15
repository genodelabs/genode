/*
 * \brief  Minimalistic implementation of the Genode::env
 * \author Norman Feske
 * \date   2014-10-14
 */

/*
 * Copyright (C) 2014 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#include <base/env.h>


namespace Genode { struct Mini_env; }


struct Genode::Mini_env : Env
{
	Parent                *parent()          { return nullptr; }
	Ram_session           *ram_session()     { return nullptr; }
	Ram_session_capability ram_session_cap() { return Ram_session_capability(); }
	Cpu_session           *cpu_session()     { return nullptr; }
	Cpu_session_capability cpu_session_cap() { return Cpu_session_capability(); }
	Rm_session            *rm_session()      { return nullptr; }
	Pd_session            *pd_session()      { return nullptr; }
	Allocator             *heap()            { return nullptr; }
};

namespace Genode {

	Env *env() {
		static Mini_env inst;
		return &inst;
	}
}
