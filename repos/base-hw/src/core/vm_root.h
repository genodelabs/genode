/*
 * \brief  base-hw specific Vm root interface
 * \author Stefan Kalkowski
 * \author Benjamin Lamowski
 * \date   2012-10-08
 */

/*
 * Copyright (C) 2012-2024 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _CORE__INCLUDE__VM_ROOT_H_
#define _CORE__INCLUDE__VM_ROOT_H_

/* Genode includes */
#include <base/heap.h>
#include <base/session_object.h>
#include <root/component.h>
#include <trace/source_registry.h>
#include <vm_session/vm_session.h>

/* core includes */
#include <vmid_allocator.h>

namespace Core {
	class Vm_root;
	using namespace Genode;
}

class Core::Vm_root : public Root_component<Session_object<Vm_session>>
{
	private:

		Ram_allocator          &_ram_allocator;
		Local_rm               &_local_rm;
		Trace::Source_registry &_trace_sources;
		Vmid_allocator          _vmid_alloc { };

	protected:

		Create_result _create_session(const char *args) override;

		void _upgrade_session(Session_object<Vm_session> &vm, const char *args) override
		{
			vm.upgrade(ram_quota_from_args(args));
			vm.upgrade(cap_quota_from_args(args));
		}

	public:

		/**
		 * Constructor
		 *
		 * \param session_ep  entrypoint managing vm_session components
		 * \param md_alloc    meta-data allocator to be used by root component
		 */
		Vm_root(Rpc_entrypoint         &session_ep,
		        Allocator              &md_alloc,
		        Ram_allocator          &ram_alloc,
		        Local_rm               &local_rm,
		        Trace::Source_registry &trace_sources)
		:
			Root_component<Session_object<Vm_session>>(&session_ep, &md_alloc),
			_ram_allocator(ram_alloc),
			_local_rm(local_rm),
			_trace_sources(trace_sources)
		{ }
};

#endif /* _CORE__INCLUDE__VM_ROOT_H_ */
