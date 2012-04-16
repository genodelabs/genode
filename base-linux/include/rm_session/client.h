/*
 * \brief  Pseudo RM-session client stub targeting the process-local RM service
 * \author Norman Feske
 * \date   2011-11-21
 */

/*
 * Copyright (C) 2011-2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__RM_SESSION__CLIENT_H_
#define _INCLUDE__RM_SESSION__CLIENT_H_

#include <base/local_interface.h>
#include <rm_session/capability.h>

namespace Genode {

	struct Rm_session_client : Rm_session
	{
		Rm_session_capability const _cap;

		/**
		 * Return pointer to locally implemented RM session
		 *
		 * \throw Local_interface::Non_local_capability
		 */
		Rm_session *_local() const { return Local_interface::deref(_cap); }

		explicit Rm_session_client(Rm_session_capability session)
		: _cap(session) { }

		Local_addr attach(Dataspace_capability ds, size_t size, off_t offset,
		                  bool use_local_addr, Local_addr local_addr,
		                  bool executable = false)
		{
			return _local()->attach(ds, size, offset, use_local_addr,
			                        local_addr, executable);
		}

		void detach(Local_addr local_addr) {
			return _local()->detach(local_addr); }

		Pager_capability add_client(Thread_capability thread) {
			return _local()->add_client(thread); }

		void fault_handler(Signal_context_capability handler) {
			return _local()->fault_handler(handler); }

		State state() {
			return _local()->state(); }

		Dataspace_capability dataspace() {
			return _local()->dataspace(); }
	};
}

#endif /* _INCLUDE__RM_SESSION__CLIENT_H_ */
