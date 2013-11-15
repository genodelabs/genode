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
#include <base/pager.h>
#include <base/printf.h>

/* base-hw includes */
#include <placement_new.h>

using namespace Genode;


/*************
 ** Mapping **
 *************/

Mapping::Mapping(addr_t const va, addr_t const pa, bool const wc,
                 bool const io, unsigned const sl2, bool const w)
:
	virt_address(va), phys_address(pa), write_combined(wc),
	io_mem(io), size_log2(sl2), writable(w)
{ }


Mapping::Mapping()
:
	virt_address(0), phys_address(0), write_combined(0),
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

Signal * Pager_object::_signal() const { return (Signal *)_signal_buf; }

void Pager_object::wake_up() { fault_resolved(); }

void Pager_object::exception_handler(Signal_context_capability) { }

void Pager_object::fault_resolved() { _signal()->~Signal(); }

unsigned Pager_object::badge() const { return _badge; }


void Pager_object::fault_occured(Signal const & s)
{
	new (_signal()) Signal(s);
}


void Pager_object::cap(Native_capability const & c)
{
	Object_pool<Pager_object>::Entry::cap(c);
}


Pager_object::Pager_object(unsigned const badge, Affinity::Location)
:
	_badge(badge)
{ }


unsigned Pager_object::signal_context_id() const
{
	return _signal_context_cap.dst();
}


/***************************
 ** Pager_activation_base **
 ***************************/

void Pager_activation_base::ep(Pager_entrypoint * const ep) { _ep = ep; }


Pager_activation_base::Pager_activation_base(char const * const name,
                                             size_t const stack_size)
:
	Thread_base(name, stack_size), _cap_valid(Lock::LOCKED), _ep(0)
{ }


Native_capability Pager_activation_base::cap()
{
	if (!_cap.valid()) { _cap_valid.lock(); }
	return _cap;
}


/**********************
 ** Pager_entrypoint **
 **********************/

void Pager_entrypoint::dissolve(Pager_object * const o) { remove_locked(o); }


Pager_entrypoint::Pager_entrypoint(Cap_session *,
                                   Pager_activation_base * const activation)
:
	_activation(activation)
{
	_activation->ep(this);
}


Pager_capability Pager_entrypoint::manage(Pager_object * const o)
{
	if (!_activation) return Pager_capability();
	o->_signal_context_cap = _activation->Signal_receiver::manage(o);
	unsigned const dst = _activation->cap().dst();
	Native_capability c = Native_capability(dst, o->badge());
	o->cap(c);
	insert(o);
	return reinterpret_cap_cast<Pager_object>(c);
}
