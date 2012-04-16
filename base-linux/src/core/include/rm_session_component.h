/*
 * \brief  Core-specific instance of the RM session interface
 * \author Christian Helmuth
 * \date   2006-07-17
 *
 * Dummies for Linux platform
 */

/*
 * Copyright (C) 2006-2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _CORE__INCLUDE__LINUX__RM_SESSION_COMPONENT_H_
#define _CORE__INCLUDE__LINUX__RM_SESSION_COMPONENT_H_

/* Genode */
#include <base/pager.h>
#include <base/allocator.h>
#include <base/rpc_server.h>
#include <rm_session/rm_session.h>

namespace Genode {

	struct Rm_client;

	class Rm_session_component : public Rpc_object<Rm_session>
	{
		public:

			Rm_session_component(Rpc_entrypoint    *ds_ep,
			                     Rpc_entrypoint    *thread_ep,
			                     Allocator         *md_alloc,
			                     size_t             ram_quota,
			                     Pager_entrypoint  *pager_ep,
			                     addr_t             vm_start,
			                     size_t             vm_size) { }

			void upgrade_ram_quota(size_t ram_quota) { }

			Local_addr attach(Dataspace_capability, size_t, off_t, bool, Local_addr, bool) {
				return (addr_t)0; }

			void detach(Local_addr) { }

			Pager_capability add_client(Thread_capability) {
				return Pager_capability(); }

			void fault_handler(Signal_context_capability) { }

			State state() { return State(); }

			Dataspace_capability dataspace() { return Dataspace_capability(); }

			void dissolve(Rm_client *cl) { }
	};

	struct Rm_member { Rm_session_component *member_rm_session() { return 0; } };
	struct Rm_client : Pager_object, Rm_member { };
}

#endif /* _CORE__INCLUDE__LINUX__RM_SESSION_COMPONENT_H_ */
