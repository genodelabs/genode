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

namespace Lx_kit {

	class Initcalls;
	class Pci_fixup_calls;
}


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


void Lx_kit::Pci_fixup_calls::add(void (*fn)(struct pci_dev*)) {
	_call_list.insert(new (_heap) E(fn)); }


void Lx_kit::Pci_fixup_calls::execute(struct pci_dev *pci_dev)
{
	for (E * entry = _call_list.first(); entry; entry = entry->next())
		entry->call(pci_dev);
}


void Lx_kit::initialize(Genode::Env & env, Genode::Signal_context & sig_ctx)
{
	Lx_kit::Env::initialize(env, sig_ctx);
}
