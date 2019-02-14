/*
 * \brief  Vm root interface
 * \author Stefan Kalkowski
 * \date   2012-10-08
 */

/*
 * Copyright (C) 2012-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _CORE__INCLUDE__VM_ROOT_H_
#define _CORE__INCLUDE__VM_ROOT_H_

/* Genode includes */
#include <root/component.h>
#include <base/heap.h>

/* core includes */
#include <vm_session_component.h>

namespace Genode { class Vm_root; }

class Genode::Vm_root : public Root_component<Vm_session_component>
{
	private:

		Ram_allocator    &_ram_allocator;
		Region_map       &_local_rm;

	protected:

		Vm_session_component *_create_session(const char *args) override
		{
			return new (md_alloc())
				Vm_session_component(*ep(),
				                     session_resources_from_args(args),
				                     session_label_from_args(args),
				                     session_diag_from_args(args),
				                     _ram_allocator, _local_rm);
		}

		void _upgrade_session(Vm_session_component *vm, const char *args) override
		{
			vm->upgrade(ram_quota_from_args(args));
			vm->upgrade(cap_quota_from_args(args));
		}

	public:

		/**
		 * Constructor
		 *
		 * \param session_ep  entrypoint managing vm_session components
		 * \param md_alloc    meta-data allocator to be used by root component
		 */
		Vm_root(Rpc_entrypoint &session_ep,
		        Allocator      &md_alloc,
		        Ram_allocator  &ram_alloc,
		        Region_map     &local_rm)
		: Root_component<Vm_session_component>(&session_ep, &md_alloc),
		 _ram_allocator(ram_alloc),
		 _local_rm(local_rm)
		{ }
};

#endif /* _CORE__INCLUDE__VM_ROOT_H_ */
