/*
 * \brief  PD service used by Noux processes
 * \author Norman Feske
 * \date   2016-04-20
 */

/*
 * Copyright (C) 2016-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _NOUX__PD_SESSION_COMPONENT_H_
#define _NOUX__PD_SESSION_COMPONENT_H_

/* Genode includes */
#include <pd_session/connection.h>
#include <base/rpc_server.h>
#include <base/env.h>

/* Noux includes */
#include <region_map_component.h>

namespace Noux { class Pd_session_component; }


class Noux::Pd_session_component : public Rpc_object<Pd_session>
{
	private:

		Rpc_entrypoint &_ep;

		Pd_connection _pd;

		Region_map_component _address_space;
		Region_map_component _stack_area;
		Region_map_component _linker_area;

	public:

		/**
		 * Constructor
		 */
		Pd_session_component(Allocator &alloc, Env &env, Rpc_entrypoint &ep,
		                     Child_policy::Name const &name,
		                     Dataspace_registry &ds_registry)
		:
			_ep(ep), _pd(env, name.string()),
			_address_space(alloc, _ep, ds_registry, _pd, _pd.address_space()),
			_stack_area   (alloc, _ep, ds_registry, _pd, _pd.stack_area()),
			_linker_area  (alloc, _ep, ds_registry, _pd, _pd.linker_area())
		{
			_ep.manage(this);
		}

		~Pd_session_component()
		{
			_ep.dissolve(this);
		}

		Pd_session_capability core_pd_cap() { return _pd.cap(); }

		void poke(Region_map &rm, addr_t dst_addr, char const *src, size_t len)
		{
			_address_space.poke(rm, dst_addr, src, len);
		}

		Capability<Region_map> lookup_region_map(addr_t const addr)
		{
			return _address_space.lookup_region_map(addr);
		}

		Region_map &address_space_region_map() { return _address_space; }
		Region_map &linker_area_region_map()   { return _linker_area;   }
		Region_map &stack_area_region_map()    { return _stack_area;    }

		void replay(Ram_session          &dst_ram,
		            Pd_session_component &dst_pd,
		            Region_map           &local_rm,
		            Allocator            &alloc,
		            Dataspace_registry   &ds_registry,
		            Rpc_entrypoint       &ep)
		{
			/* replay region map into new protection domain */
			_stack_area   .replay(dst_ram, dst_pd.stack_area_region_map(),    local_rm, alloc, ds_registry, ep);
			_linker_area  .replay(dst_ram, dst_pd.linker_area_region_map(),   local_rm, alloc, ds_registry, ep);
			_address_space.replay(dst_ram, dst_pd.address_space_region_map(), local_rm, alloc, ds_registry, ep);

			Region_map &dst_address_space = dst_pd.address_space_region_map();
			Region_map &dst_stack_area    = dst_pd.stack_area_region_map();
			Region_map &dst_linker_area   = dst_pd.linker_area_region_map();

			/* attach stack area */
			dst_address_space.attach(dst_stack_area.dataspace(),
			                         Dataspace_client(dst_stack_area.dataspace()).size(),
			                         0, true,
			                         _address_space.lookup_region_base(_stack_area.dataspace()));

			/* attach linker area */
			dst_address_space.attach(dst_linker_area.dataspace(),
			                         Dataspace_client(dst_linker_area.dataspace()).size(),
			                         0, true,
			                         _address_space.lookup_region_base(_linker_area.dataspace()));
		}


		/**************************
		 ** Pd_session interface **
		 **************************/

		void assign_parent(Capability<Parent> parent) override {
			_pd.assign_parent(parent); }

		bool assign_pci(addr_t addr, uint16_t bdf) override {
			return _pd.assign_pci(addr, bdf); }

		Signal_source_capability alloc_signal_source() override {
			return _pd.alloc_signal_source(); }

		void free_signal_source(Signal_source_capability cap) override {
			_pd.free_signal_source(cap); }

		Capability<Signal_context> alloc_context(Signal_source_capability source,
		                                         unsigned long imprint) override {
			return _pd.alloc_context(source, imprint); }

		void free_context(Capability<Signal_context> cap) override {
			_pd.free_context(cap); }

		void submit(Capability<Signal_context> context, unsigned cnt) override {
			_pd.submit(context, cnt); }

		Native_capability alloc_rpc_cap(Native_capability ep) override {
			return _pd.alloc_rpc_cap(ep); }

		void free_rpc_cap(Native_capability cap) override {
			_pd.free_rpc_cap(cap); }

		Capability<Region_map> address_space() override {
			return _address_space.Rpc_object<Region_map>::cap(); }

		Capability<Region_map> stack_area() override {
			return _stack_area.Rpc_object<Region_map>::cap(); }

		Capability<Region_map> linker_area() override {
			return _linker_area.Rpc_object<Region_map>::cap(); }

		Capability<Native_pd> native_pd() override {
			return _pd.native_pd(); }
};

#endif /* _NOUX__PD_SESSION_COMPONENT_H_ */
