/*
 * \brief  Core-specific instance of the region-map interface
 * \author Christian Helmuth
 * \date   2006-07-17
 *
 * Dummies for Linux platform
 */

/*
 * Copyright (C) 2006-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _CORE__INCLUDE__REGION_MAP_COMPONENT_H_
#define _CORE__INCLUDE__REGION_MAP_COMPONENT_H_

/* Genode includes */
#include <util/list.h>
#include <base/allocator.h>
#include <base/rpc_server.h>
#include <base/session_label.h>
#include <region_map/region_map.h>

/* core includes */
#include <pager.h>
#include <platform_thread.h>

namespace Genode {
	struct Rm_client;
	struct Rm_member;
	class Region_map_component;
}


class Genode::Region_map_component : public Rpc_object<Region_map>,
                                     public List<Region_map_component>::Element
{
	private:

		struct Rm_dataspace_component { void sub_rm(Native_capability) { } };

	public:

		Region_map_component(Rpc_entrypoint &, Allocator &, Pager_entrypoint &,
		                     addr_t, size_t) { }

		void upgrade_ram_quota(size_t ram_quota) { }

		void add_client(Rm_client &) { }
		void remove_client(Rm_client &) { }

		Local_addr attach(Dataspace_capability, size_t, off_t, bool, Local_addr, bool) {
			return (addr_t)0; }

		void detach(Local_addr) { }

		void fault_handler(Signal_context_capability) { }

		State state() { return State(); }

		Dataspace_capability dataspace() { return Dataspace_capability(); }

		Rm_dataspace_component *dataspace_component() { return 0; }

		void address_space(Platform_pd *) { }
};


struct Genode::Rm_member { Region_map_component *member_rm() { return 0; } };


struct Genode::Rm_client : Pager_object, Rm_member
{
	Rm_client(Cpu_session_capability, Thread_capability, 
	          Region_map_component *rm, unsigned long badge,
	          Affinity::Location location, Cpu_session::Name const&,
	          Session_label const&)
	{ }
};

#endif /* _CORE__INCLUDE__REGION_MAP_COMPONENT_H_ */
