/*
 * \brief  Lx_kit backend for Linux kernel initialization
 * \author Stefan Kalkowski
 * \date   2021-03-10
 */

/*
 * Copyright (C) 2021 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

#include <base/log.h>

#include <lx_kit/env.h>
#include <lx_kit/init.h>

namespace Lx_kit { class Initcalls; }


void Lx_kit::Initcalls::add(int (*initcall)(void), unsigned int prio) {
	_call_list.insert(new (_heap) E(prio, initcall)); }


void Lx_kit::Initcalls::execute_in_order()
{
	unsigned min = ~0U;
	unsigned max = 0;
	for (E * entry = _call_list.first(); entry; entry = entry->next()) {
		if (entry->prio < min) min = entry->prio;
		if (entry->prio > max) max = entry->prio;
	}

	for (unsigned i = min; i <= max; i++) {
		for (E * entry = _call_list.first(); entry; entry = entry->next()) {
			if (entry->prio == i) entry->call();
		}
	}
}


void Lx_kit::initialize(Genode::Env & env)
{
	Lx_kit::env(&env);
	env.exec_static_constructors();
}
