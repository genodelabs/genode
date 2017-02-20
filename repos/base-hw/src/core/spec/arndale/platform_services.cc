/*
 * \brief   Platform specific services for Arndale
 * \author  Stefan Kalkowski
 * \date    2014-07-08
 */

/*
 * Copyright (C) 2014-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <base/service.h>
#include <base/heap.h>

/* core includes */
#include <platform_services.h>
#include <core_parent.h>        /* for 'Core_service' type */
#include <map_local.h>
#include <vm_root.h>

extern Genode::addr_t _vt_host_context_ptr;
extern Genode::addr_t _vt_host_entry;
extern Genode::addr_t _mt_master_context_begin;

/*
 * Add ARM virtualization specific vm service
 */
void Genode::platform_add_local_services(Rpc_entrypoint *ep,
                                         Sliced_heap *sh,
                                         Registry<Service> *services)
{
	using namespace Genode;

	/* initialize host context used in virtualization world switch */
	*((void**)&_vt_host_context_ptr) = &_mt_master_context_begin;

	map_local(Platform::core_phys_addr((addr_t)&_vt_host_entry), 0xfff00000, 1, PAGE_FLAGS_KERN_TEXT);

	static Vm_root vm_root(ep, sh);
	static Core_service<Vm_session_component> vm_service(*services, vm_root);
}
