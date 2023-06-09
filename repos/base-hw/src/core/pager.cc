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

using namespace Core;


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


void Pager_object::start_paging(Kernel_object<Kernel::Signal_receiver> & receiver)
{
	using Object = Kernel_object<Kernel::Signal_context>;
	using Entry  = Object_pool<Pager_object>::Entry;

	create(*receiver, (unsigned long)this);
	Entry::cap(Object::_cap);
}


void Pager_object::exception_handler(Signal_context_capability) { }


void Pager_object::unresolved_page_fault_occurred() { }


void Pager_object::print(Output &out) const
{
	Platform_thread * const pt = (Platform_thread *)badge();
	if (pt && pt->pd())
		Genode::print(out, "pager_object: pd='", pt->pd()->label(),
		                   "' thread='", pt->label(), "'");
}


Pager_object::Pager_object(Cpu_session_capability cpu_session_cap,
                           Thread_capability thread_cap, addr_t const badge,
                           Affinity::Location, Session_label const &,
                           Cpu_session::Name const &)
:
	Object_pool<Pager_object>::Entry(Kernel_object<Kernel::Signal_context>::_cap),
	_badge(badge), _cpu_session_cap(cpu_session_cap), _thread_cap(thread_cap)
{ }


/**********************
 ** Pager_entrypoint **
 **********************/

void Pager_entrypoint::dissolve(Pager_object &o)
{
	Kernel::kill_signal_context(Capability_space::capid(o.cap()));
	remove(&o);
}


Pager_entrypoint::Pager_entrypoint(Rpc_cap_factory &)
:
	Thread(Weight::DEFAULT_WEIGHT, "pager_ep", PAGER_EP_STACK_SIZE,
	       Type::NORMAL),

	_kobj(_kobj.CALLED_FROM_CORE)
{
	start();
}


Pager_capability Pager_entrypoint::manage(Pager_object &o)
{
	o.start_paging(_kobj);
	insert(&o);
	return reinterpret_cap_cast<Pager_object>(o.cap());
}
