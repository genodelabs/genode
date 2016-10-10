/**
 * \brief  Core implementation of the CPU session interface extension
 * \author Stefan Kalkowski
 * \date   2011-04-04
 */

/*
 * Copyright (C) 2011-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <base/log.h>

/* core includes */
#include <native_cpu_component.h>
#include <cpu_session_irqs.h>
#include <platform.h>

/* Fiasco.OC includes */
namespace Fiasco {
#include <l4/sys/thread.h>
#include <l4/sys/factory.h>
}

static Genode::Avl_tree<Genode::Cpu_session_irqs> _irq_tree;



void Genode::Native_cpu_component::enable_vcpu(Genode::Thread_capability thread_cap,
                                                Genode::addr_t vcpu_state)
{
	using namespace Genode;
	using namespace Fiasco;

	auto lambda = [&] (Cpu_thread_component *thread) {
		if (!thread) return;

		l4_cap_idx_t tid = thread->platform_thread().thread().local.data()->kcap();

		l4_msgtag_t tag = l4_thread_vcpu_control(tid, vcpu_state);
		if (l4_msgtag_has_error(tag))
			warning("l4_thread_vcpu_control failed");
	};
	_thread_ep.apply(thread_cap, lambda);
}


Genode::Native_capability
Genode::Native_cpu_component::native_cap(Genode::Thread_capability cap)
{
	using namespace Genode;

	auto lambda = [&] (Cpu_thread_component *thread) {
		return (!thread) ? Native_capability()
		                 : thread->platform_thread().thread().local;
	};
	return _thread_ep.apply(cap, lambda);
}


Genode::Native_capability Genode::Native_cpu_component::alloc_irq()
{
	using namespace Fiasco;
	using namespace Genode;

	/* find irq object container of this cpu-session */
	Cpu_session_irqs* node = _irq_tree.first();
	if (node)
		node = node->find_by_session(&_cpu_session);

	/* if not found, we've to create one */
	if (!node) {
		node = new (&_cpu_session._md_alloc) Cpu_session_irqs(&_cpu_session);
		_irq_tree.insert(node);
	}

	/* construct irq kernel-object */
	Cap_index* i = cap_map()->insert(platform_specific()->cap_id_alloc()->alloc());
	l4_msgtag_t res = l4_factory_create_irq(L4_BASE_FACTORY_CAP, i->kcap());
	if (l4_error(res)) {
		warning("Allocation of irq object failed!");
		return Genode::Native_capability();
	}

	/* construct cap and hold a reference in the irq container object */
	Genode::Native_capability cap(*i);
	return (node->add(cap)) ? cap : Genode::Native_capability();
}


Genode::Native_cpu_component::Native_cpu_component(Cpu_session_component &cpu_session, char const *)
:
	_cpu_session(cpu_session), _thread_ep(*_cpu_session._thread_ep)
{
	_thread_ep.manage(this);
}


Genode::Native_cpu_component::~Native_cpu_component()
{
	_thread_ep.dissolve(this);
}
