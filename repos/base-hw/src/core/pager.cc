/*
 * \brief  Pager implementations that are specific for the HW-core
 * \author Martin Stein
 * \date   2012-03-29
 */

/*
 * Copyright (C) 2012-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* core includes*/
#include <pager.h>
#include <platform_thread.h>
#include <platform_pd.h>
#include <region_map_component.h>

/* base-internal includes */
#include <base/internal/capability_space.h>
#include <base/internal/native_thread.h>

using namespace Core;

static unsigned _nr_of_cpus = 0;
static void    *_pager_thread_memory = nullptr;


void Core::init_pager_thread_per_cpu_memory(unsigned const cpus, void * mem)
{
	_nr_of_cpus = cpus;
	_pager_thread_memory = mem;
}


void Core::init_page_fault_handling(Rpc_entrypoint &) { }


/*************
 ** Mapping **
 *************/

void Mapping::prepare_map_operation() const { }


/***************
 ** Ipc_pager **
 ***************/

addr_t Ipc_pager::fault_ip() const { return _fault.ip; }

addr_t Ipc_pager::fault_addr() const { return _fault.addr; }

bool Ipc_pager::write_fault() const {
	return _fault.type == Kernel::Thread_fault::WRITE; }

bool Ipc_pager::exec_fault() const {
	return _fault.type == Kernel::Thread_fault::EXEC; }

void Ipc_pager::set_reply_mapping(Mapping m) { _mapping = m; }


/******************
 ** Pager_object **
 ******************/

void Pager_object::wake_up()
{
	Platform_thread * const pt = (Platform_thread *)badge();
	if (pt) pt->restart();
}


void Pager_object::start_paging(Kernel_object<Kernel::Signal_receiver> &receiver,
                                Platform_thread &pager_thread)
{
	using Object = Kernel_object<Kernel::Signal_context>;
	using Entry  = Object_pool<Pager_object>::Entry;

	create(*receiver, (unsigned long)this);
	Entry::cap(Object::_cap);
	_pager_thread = &pager_thread;
}


void Pager_object::unresolved_page_fault_occurred() { }


void Pager_object::print(Output &out) const
{
	Platform_thread * const pt = (Platform_thread *)badge();
	if (pt)
		Genode::print(out, "pager_object: pd='", pt->pd().label(),
		                   "' thread='", pt->label(), "'");
}


Pager_object::Pager_object(Cpu_session_capability cpu_session_cap,
                           Thread_capability thread_cap, addr_t const badge,
                           Affinity::Location location, Session_label const &,
                           Cpu_session::Name const &)
:
	Object_pool<Pager_object>::Entry(Kernel_object<Kernel::Signal_context>::_cap),
	_badge(badge), _location(location),
	_cpu_session_cap(cpu_session_cap), _thread_cap(thread_cap)
{ }


/**********************
 ** Pager_entrypoint **
 **********************/

void Pager_entrypoint::Thread::entry()
{
	while (1) {

		/* receive fault */
		if (Kernel::await_signal(Capability_space::capid(_kobj.cap())))
			continue;

		Pager_object *po = *(Pager_object**)Thread::myself()->utcb()->data();
		if (!po)
			continue;

		Untyped_capability cap = po->cap();

		/* fetch fault data */
		Platform_thread * const pt = (Platform_thread *)po->badge();
		if (!pt) {
			warning("failed to get platform thread of faulter");
			Kernel::ack_signal(Capability_space::capid(cap));
			continue;
		}

		if (pt->exception_state() ==
		    Kernel::Thread::Exception_state::EXCEPTION) {
			if (!po->submit_exception_signal())
				warning("unresolvable exception: "
				        "pd='",     pt->pd().label(), "', "
				        "thread='", pt->label(),       "', "
				        "ip=",      Hex(pt->state().cpu.ip));
			pt->fault_resolved(cap, false);
			continue;
		}

		_fault = pt->fault_info();

		/* try to resolve fault directly via local region managers */
		if (po->pager(*this) == Pager_object::Pager_result::STOP) {
			pt->fault_resolved(cap, false);
			continue;
		}

		/* apply mapping that was determined by the local region managers */
		{
			Locked_ptr<Address_space> locked_ptr(pt->address_space());
			if (!locked_ptr.valid()) {
				pt->fault_resolved(cap, false);
				continue;
			}

			Hw::Address_space * as = static_cast<Hw::Address_space*>(&*locked_ptr);

			Cache cacheable = Genode::CACHED;
			if (!_mapping.cached)
				cacheable = Genode::UNCACHED;
			if (_mapping.write_combined)
				cacheable = Genode::WRITE_COMBINED;

			Hw::Page_flags const flags {
				.writeable  = _mapping.writeable  ? Hw::RW   : Hw::RO,
				.executable = _mapping.executable ? Hw::EXEC : Hw::NO_EXEC,
				.privileged = Hw::USER,
				.global     = Hw::NO_GLOBAL,
				.type       = _mapping.io_mem ? Hw::DEVICE : Hw::RAM,
				.cacheable  = cacheable
			};

			as->insert_translation(_mapping.dst_addr, _mapping.src_addr,
			                       1UL << _mapping.size_log2, flags);
		}

		pt->fault_resolved(cap, true);
	}
}


Pager_entrypoint::Thread::Thread(Affinity::Location cpu)
:
	Genode::Thread(Weight::DEFAULT_WEIGHT, "pager_ep", PAGER_EP_STACK_SIZE, cpu),
	_kobj(_kobj.CALLED_FROM_CORE)
{
	start();
}


void Pager_entrypoint::dissolve(Pager_object &o)
{
	Kernel::kill_signal_context(Capability_space::capid(o.cap()));

	unsigned const cpu = o.location().xpos();
	if (cpu >= _cpus)
		error("Invalid location of pager object ", cpu);
	else
		_threads[cpu].remove(&o);
}


Pager_capability Pager_entrypoint::manage(Pager_object &o)
{
	unsigned const cpu = o.location().xpos();
	if (cpu >= _cpus) {
		error("Invalid location of pager object ", cpu);
	} else {
		o.start_paging(_threads[cpu]._kobj,
		               *_threads[cpu].native_thread().platform_thread);
		_threads[cpu].insert(&o);
	}

	return reinterpret_cap_cast<Pager_object>(o.cap());
}


Pager_entrypoint::Pager_entrypoint(Rpc_cap_factory &)
:
	_cpus(_nr_of_cpus),
	_threads((Thread*)_pager_thread_memory)
{
	for (unsigned i = 0; i < _cpus; i++)
		construct_at<Thread>((void*)&_threads[i], Affinity::Location(i, 0));
}
