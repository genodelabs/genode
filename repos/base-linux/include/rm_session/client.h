/*
 * \brief  Pseudo RM-session client stub targeting the process-local RM service
 * \author Norman Feske
 * \date   2011-11-21
 */

/*
 * Copyright (C) 2011-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__RM_SESSION__CLIENT_H_
#define _INCLUDE__RM_SESSION__CLIENT_H_

/* Genode includes */
#include <base/local_capability.h>
#include <rm_session/capability.h>

namespace Genode {

	struct Rm_session_client : Rm_session, Rm_session_capability
	{
		typedef Rm_session Rpc_interface;

		/**
		 * Return pointer to locally implemented RM session
		 *
		 * \throw Local_interface::Non_local_capability
		 */
		Rm_session *_local() const {
			return Local_capability<Rm_session>::deref(*this); }

		explicit Rm_session_client(Rm_session_capability session)
		: Rm_session_capability(session) { }

		Local_addr attach(Dataspace_capability ds, size_t size = 0,
		                  off_t offset = 0, bool use_local_addr = false,
		                  Local_addr local_addr = (void *)0,
		                  bool executable = false)
		{
			return _local()->attach(ds, size, offset, use_local_addr,
			                        local_addr, executable);
		}

		void detach(Local_addr local_addr) {
			return _local()->detach(local_addr); }

		Pager_capability add_client(Thread_capability thread) {
			return _local()->add_client(thread); }

		void remove_client(Pager_capability pager) {
			_local()->remove_client(pager); }

		void fault_handler(Signal_context_capability /*handler*/)
		{
			/*
			 * On Linux, page faults are never reflected to RM clients. They
			 * are always handled by the kernel. If a segmentation fault
			 * occurs, this condition is being reflected as a CPU exception
			 * to the handler registered via 'Cpu_session::exception_handler'.
			 */
		}

		State state() {
			return _local()->state(); }

		Dataspace_capability dataspace() {
			return _local()->dataspace(); }
	};
}

#endif /* _INCLUDE__RM_SESSION__CLIENT_H_ */
