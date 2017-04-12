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

#ifndef _CORE__SIGNAL_SOURCE_COMPONENT_H_
#define _CORE__SIGNAL_SOURCE_COMPONENT_H_

/* Genode includes */
#include <base/object_pool.h>

/* core-local includes */
#include <object.h>
#include <kernel/signal_receiver.h>
#include <assertion.h>

namespace Genode {

	class Signal_context_component;
	class Signal_source_component;

	typedef Object_pool<Signal_context_component> Signal_context_pool;
	typedef Object_pool<Signal_source_component>  Signal_source_pool;
}


struct Genode::Signal_context_component : Kernel_object<Kernel::Signal_context>,
                                          Signal_context_pool::Entry
{
	inline Signal_context_component(Signal_source_component &s,
	                                unsigned long const imprint);

	Signal_source_component *source() { ASSERT_NEVER_CALLED; }
};


struct Genode::Signal_source_component : Kernel_object<Kernel::Signal_receiver>,
                                         Signal_source_pool::Entry
{
	Signal_source_component()
	:
		Kernel_object<Kernel::Signal_receiver>(true),
		Signal_source_pool::Entry(Kernel_object<Kernel::Signal_receiver>::_cap)
	{ }

	void submit(Signal_context_component *, unsigned long) { ASSERT_NEVER_CALLED; }
};


Genode::Signal_context_component::Signal_context_component(Signal_source_component &s,
                                                           unsigned long const imprint)
:
	Kernel_object<Kernel::Signal_context>(true, s.kernel_object(), imprint),
	Signal_context_pool::Entry(Kernel_object<Kernel::Signal_context>::_cap)
{ }

#endif /* _CORE__SIGNAL_SOURCE_COMPONENT_H_ */
