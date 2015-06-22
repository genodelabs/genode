/*
 * \brief  Pager implementations that are specific for the HW-core
 * \author Martin Stein
 * \date   2012-03-29
 */

/*
 * Copyright (C) 2012-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <base/printf.h>

/* core includes*/
#include <pager.h>
#include <platform_thread.h>
#include <platform_pd.h>

using namespace Genode;


/*************
 ** Mapping **
 *************/

Mapping::Mapping(addr_t const va, addr_t const pa,
                 Cache_attribute const c, bool const io,
                 unsigned const sl2, bool const w)
:
	virt_address(va), phys_address(pa), cacheable(c),
	io_mem(io), size_log2(sl2), writable(w)
{ }


Mapping::Mapping()
:
virt_address(0), phys_address(0), cacheable(CACHED),
	io_mem(0), size_log2(0), writable(0)
{ }


void Mapping::prepare_map_operation() { }


/***************
 ** Ipc_pager **
 ***************/

addr_t Ipc_pager::fault_ip() const { return _fault.ip; }

addr_t Ipc_pager::fault_addr() const { return _fault.addr; }

bool Ipc_pager::is_write_fault() const { return _fault.writes; }

void Ipc_pager::set_reply_mapping(Mapping m) { _mapping = m; }


/******************
 ** Pager_object **
 ******************/

Thread_capability Pager_object::thread_cap() const { return _thread_cap; }

void Pager_object::thread_cap(Thread_capability const & c) { _thread_cap = c; }

void Pager_object::wake_up()
{
	using Object = Kernel_object<Kernel::Signal_context>;
	Kernel::ack_signal(Object::_cap.dst());
}

void Pager_object::start_paging(Kernel::Signal_receiver * receiver)
{
	using Object = Kernel_object<Kernel::Signal_context>;
	using Entry  = Object_pool<Pager_object>::Entry;

	create(receiver, (unsigned long)this);
	Entry::cap(Object::_cap);
}

void Pager_object::exception_handler(Signal_context_capability) { }

void Pager_object::unresolved_page_fault_occurred()
{
	Platform_thread * const pt = (Platform_thread *)badge();
	if (pt && pt->pd())
		PERR("%s -> %s: unresolved pagefault at ip=%p sp=%p",
		     pt->pd()->label(), pt->label(), (void*)pt->state().ip, (void*)pt->state().sp);
}

Pager_object::Pager_object(unsigned const badge, Affinity::Location)
: Object_pool<Pager_object>::Entry(Kernel_object<Kernel::Signal_context>::_cap),
  _badge(badge)
{ }


/***************************
 ** Pager_activation_base **
 ***************************/

void Pager_activation_base::ep(Pager_entrypoint * const ep) { _ep = ep; }


Pager_activation_base::Pager_activation_base(char const * const name,
                                             size_t const stack_size)
: Thread_base(0, name, stack_size),
  Kernel_object<Kernel::Signal_receiver>(true),
  _startup_lock(Lock::LOCKED), _ep(0)
{ }


/**********************
 ** Pager_entrypoint **
 **********************/

void Pager_entrypoint::dissolve(Pager_object * const o)
{
	remove_locked(o);
}


Pager_entrypoint::Pager_entrypoint(Cap_session *,
                                   Pager_activation_base * const activation)
: _activation(activation) {
	_activation->ep(this); }


Pager_capability Pager_entrypoint::manage(Pager_object * const o)
{
	o->start_paging(_activation->kernel_object());
	insert(o);
	return reinterpret_cap_cast<Pager_object>(o->cap());
}
