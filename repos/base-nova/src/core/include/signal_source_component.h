/*
 * \brief  Signal-delivery mechanism
 * \author Norman Feske
 * \date   2009-08-05
 */

/*
 * Copyright (C) 2009-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _CORE__INCLUDE__SIGNAL_SOURCE_COMPONENT_H_
#define _CORE__INCLUDE__SIGNAL_SOURCE_COMPONENT_H_

#include <base/object_pool.h>
#include <signal_source/nova_signal_source.h>
#include <assertion.h>

namespace Genode {

	class Signal_context_component;
	class Signal_source_component;
}


struct Genode::Signal_context_component : Object_pool<Signal_context_component>::Entry
{
	Signal_context_component(Signal_context_capability cap)
	: Object_pool<Signal_context_component>::Entry(cap) { }

	Signal_source_component *source() { ASSERT_NEVER_CALLED; }
};


class Genode::Signal_source_component : public Rpc_object<Nova_signal_source,
                                                          Signal_source_component>
{
	private:

		Native_capability _blocking_semaphore;

	public:

		Signal_source_component(Rpc_entrypoint *) { }

		void register_semaphore(Native_capability const &cap)
		{
			_blocking_semaphore = cap;
		}

		Native_capability blocking_semaphore() const { return _blocking_semaphore; }

		Signal wait_for_signal() { /* unused on NOVA */ return Signal(0, 0); }

		void submit(Signal_context_component *, unsigned long) { /* unused on NOVA */ }
};

#endif /* _CORE__INCLUDE__SIGNAL_SOURCE_COMPONENT_H_ */
