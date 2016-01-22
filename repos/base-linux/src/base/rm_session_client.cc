/*
 * \brief  Pseudo RM-session client stub targeting the process-local RM service
 * \author Norman Feske
 * \date   2011-11-21
 */

/*
 * Copyright (C) 2011-2016 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#include <base/local_capability.h>
#include <rm_session/client.h>

using namespace Genode;

/**
 * Return pointer to locally implemented RM session
 *
 * \throw Local_interface::Non_local_capability
 */
static Rm_session *_local(Rm_session_capability cap)
{
	return Local_capability<Rm_session>::deref(cap);
}


Rm_session_client::Rm_session_client(Rm_session_capability session)
: Rpc_client<Rm_session>(session) { }


Rm_session::Local_addr
Rm_session_client::attach(Dataspace_capability ds, size_t size,
                          off_t offset, bool use_local_addr,
                          Rm_session::Local_addr local_addr,
                          bool executable)
{
	return _local(*this)->attach(ds, size, offset, use_local_addr,
	                             local_addr, executable);
}

void Rm_session_client::detach(Local_addr local_addr) {
	return _local(*this)->detach(local_addr); }


Pager_capability Rm_session_client::add_client(Thread_capability thread) {
	return _local(*this)->add_client(thread); }


void Rm_session_client::remove_client(Pager_capability pager) {
	_local(*this)->remove_client(pager); }


void Rm_session_client::fault_handler(Signal_context_capability /*handler*/)
{
	/*
	 * On Linux, page faults are never reflected to RM clients. They
	 * are always handled by the kernel. If a segmentation fault
	 * occurs, this condition is being reflected as a CPU exception
	 * to the handler registered via 'Cpu_session::exception_handler'.
	 */
}


Rm_session::State Rm_session_client::state() { return _local(*this)->state(); }


Dataspace_capability Rm_session_client::dataspace() {
	return _local(*this)->dataspace(); }

