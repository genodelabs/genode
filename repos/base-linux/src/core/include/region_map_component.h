/*
 * \brief  Core-specific instance of the region-map interface
 * \author Christian Helmuth
 * \date   2006-07-17
 *
 * Dummies for Linux platform
 */

/*
 * Copyright (C) 2006-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _CORE__INCLUDE__REGION_MAP_COMPONENT_H_
#define _CORE__INCLUDE__REGION_MAP_COMPONENT_H_

/* Genode includes */
#include <util/list.h>
#include <base/allocator.h>
#include <base/rpc_server.h>
#include <region_map/region_map.h>

/* Core includes */
#include <pager.h>

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

		Local_addr attach(Dataspace_capability, size_t, off_t, bool, Local_addr, bool) {
			return (addr_t)0; }

		void detach(Local_addr) { }

		Pager_capability add_client(Thread_capability) {
			return Pager_capability(); }

		void remove_client(Pager_capability) { }

		void fault_handler(Signal_context_capability) { }

		State state() { return State(); }

		Dataspace_capability dataspace() { return Dataspace_capability(); }

		Rm_dataspace_component *dataspace_component() { return 0; }
};


struct Genode::Rm_member { Region_map_component *member_rm() { return 0; } };


struct Genode::Rm_client : Pager_object, Rm_member { };

#endif /* _CORE__INCLUDE__REGION_MAP_COMPONENT_H_ */
