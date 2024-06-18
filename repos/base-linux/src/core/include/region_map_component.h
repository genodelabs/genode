/*
 * \brief  Core-specific instance of the region-map interface
 * \author Christian Helmuth
 * \date   2006-07-17
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

namespace Core {

	struct Rm_client;
	struct Rm_member;
	class Region_map_component;
}


class Core::Region_map_component : public Rpc_object<Region_map>,
                                   private List<Region_map_component>::Element
{
	private:

		friend class List<Region_map_component>;

		struct Rm_dataspace_component { void sub_rm(Native_capability) { } };

	public:

		Region_map_component(Rpc_entrypoint &, Allocator &, Pager_entrypoint &,
		                     addr_t, size_t, Session::Diag) { }

		void upgrade_ram_quota(size_t) { }

		void add_client(Rm_client &) { }
		void remove_client(Rm_client &) { }

		Attach_result attach(Dataspace_capability, Attr const &) override {
			return Attach_error::REGION_CONFLICT; }

		void detach(addr_t) override { }

		void fault_handler(Signal_context_capability) override { }

		Fault fault() override { return { }; }

		Dataspace_capability dataspace() override { return Dataspace_capability(); }

		Rm_dataspace_component *dataspace_component() { return nullptr; }

		void address_space(Platform_pd *) { }

		using Attach_dma_result = Pd_session::Attach_dma_result;

		Attach_dma_result attach_dma(Dataspace_capability, addr_t) {
			return Pd_session::Attach_dma_error::DENIED; };
};


struct Core::Rm_client : Pager_object
{
	Rm_client(Cpu_session_capability, Thread_capability,
	          Region_map_component &, unsigned long,
	          Affinity::Location, Cpu_session::Name const &,
	          Session_label const &)
	{ }
};

#endif /* _CORE__INCLUDE__REGION_MAP_COMPONENT_H_ */
