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
	Parent                *parent()          override { return nullptr; }
	Ram_session           *ram_session()     override { return nullptr; }
	Ram_session_capability ram_session_cap() override { return Ram_session_capability(); }
	Cpu_session           *cpu_session()     override { return nullptr; }
	Cpu_session_capability cpu_session_cap() override { return Cpu_session_capability(); }
	Rm_session            *rm_session()      override { return nullptr; }
	Pd_session            *pd_session()      override { return nullptr; }
	Allocator             *heap()            override { return nullptr; }

	void reinit(Native_capability::Dst, long)        override { }
	void reinit_main_thread(Rm_session_capability &) override { }
};

namespace Genode {

	Env *env() {
		static Mini_env inst;
		return &inst;
	}
}
