/*
 * \brief  Cpu_thread_component class for GDB monitor
 * \author Christian Prochaska
 * \date   2016-05-12
 */

/*
 * Copyright (C) 2016 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */


/* GDB monitor includes */
#include "cpu_thread_component.h"


/* mem-break.c */
extern "C" int breakpoint_len;
extern "C" const unsigned char *breakpoint_data;
extern "C" int set_gdb_breakpoint_at(long long where);

/* genode-low.cc */
extern "C" int genode_read_memory(long long memaddr, unsigned char *myaddr, int len);
extern "C" int genode_write_memory (long long memaddr, const unsigned char *myaddr, int len);
extern void genode_set_initial_breakpoint_at(long long addr);


static unsigned long new_lwpid = GENODE_MAIN_LWPID;


using namespace Gdb_monitor;


bool Cpu_thread_component::_set_breakpoint_at_first_instruction(addr_t ip)
{
	_breakpoint_ip = ip;

	if (genode_read_memory(_breakpoint_ip, _original_instructions,
	                       breakpoint_len) != 0) {
		PWRN("%s: could not read memory at thread start address", __PRETTY_FUNCTION__);
		return false;
	}

	if (genode_write_memory(_breakpoint_ip, breakpoint_data,
	                        breakpoint_len) != 0) {
		PWRN("%s: could not set breakpoint at thread start address", __PRETTY_FUNCTION__);
		return false;
	}

	return true;
}


void Cpu_thread_component::_remove_breakpoint_at_first_instruction()
{
	if (genode_write_memory(_breakpoint_ip, _original_instructions,
	                        breakpoint_len) != 0)
		PWRN("%s: could not remove breakpoint at thread start address", __PRETTY_FUNCTION__);
}


Dataspace_capability Cpu_thread_component::utcb()
{
	return _parent_cpu_thread.utcb();
}


void Cpu_thread_component::start(addr_t ip, addr_t sp)
{
	_lwpid = new_lwpid++;
	_initial_ip = ip;

	/* register the exception handler */
	exception_sigh(exception_signal_context_cap());

	/* set breakpoint at first instruction */
	if (lwpid() == GENODE_MAIN_LWPID)
		_set_breakpoint_at_first_instruction(ip);
	else
		genode_set_initial_breakpoint_at(ip);

	_parent_cpu_thread.start(ip, sp);
}


void Cpu_thread_component::pause()
{
	_parent_cpu_thread.pause();
}


void Cpu_thread_component::resume()
{
	_parent_cpu_thread.resume();
}


void Cpu_thread_component::single_step(bool enable)
{
	_parent_cpu_thread.single_step(enable);
}


void Cpu_thread_component::cancel_blocking()
{
	_parent_cpu_thread.cancel_blocking();
}


void Cpu_thread_component::state(Thread_state const &state)
{
	_parent_cpu_thread.state(state);
}


Thread_state Cpu_thread_component::state()
{
	return _parent_cpu_thread.state();
}


void Cpu_thread_component::exception_sigh(Signal_context_capability handler)
{
	_parent_cpu_thread.exception_sigh(handler);
}


void Cpu_thread_component::affinity(Affinity::Location location)
{
	_parent_cpu_thread.affinity(location);
}


unsigned Cpu_thread_component::trace_control_index()
{
	return _parent_cpu_thread.trace_control_index();
}


Dataspace_capability Cpu_thread_component::trace_buffer()
{
	return _parent_cpu_thread.trace_buffer();
}


Dataspace_capability Cpu_thread_component::trace_policy()
{
	return _parent_cpu_thread.trace_policy();
}
