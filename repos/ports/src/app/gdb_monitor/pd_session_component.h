/*
 * \brief  Core-specific instance of the PD session interface
 * \author Norman Feske
 * \date   2016-04-20
 */

/*
 * Copyright (C) 2016-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _PD_SESSION_COMPONENT_H_
#define _PD_SESSION_COMPONENT_H_

/* Genode includes */
#include <base/rpc_server.h>
#include <pd_session/connection.h>

/* GDB monitor includes */
#include "region_map_component.h"

namespace Gdb_monitor {
	using namespace Genode;

	class Pd_session_component;
	typedef Local_service<Pd_session_component> Pd_service;
}

class Gdb_monitor::Pd_session_component : public Rpc_object<Pd_session>
{
	private:

		Rpc_entrypoint &_ep;
		Allocator      &_alloc;

		Pd_connection _pd;

		Region_map_component _address_space;
		Region_map_component _stack_area;
		Region_map_component _linker_area;

	public:

		/**
		 * Constructor
		 */
		Pd_session_component(Rpc_entrypoint &ep,
		                     Env            &env,
		                     Allocator      &alloc,
		                     char const     *binary_name,
		                     Dataspace_pool &managed_ds_map)
		:
			_ep(ep),
			_alloc(alloc),
			_pd(env, binary_name),
			_address_space(_ep, _alloc, managed_ds_map, _pd, _pd.address_space()),
			_stack_area   (_ep, _alloc, managed_ds_map, _pd, _pd.stack_area()),
			_linker_area  (_ep, _alloc, managed_ds_map, _pd, _pd.linker_area())
		{
			_ep.manage(this);
		}

		~Pd_session_component()
		{
			_ep.dissolve(this);
		}

		/**
		 * Accessor used to let the GDB monitor access the PD's address
		 * space
		 */
		Region_map_component &region_map() { return _address_space; }

		Pd_session_capability core_pd_cap() { return _pd.cap(); }

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

		void ref_account(Capability<Pd_session> pd) override {
			_pd.ref_account(pd); }

		void transfer_quota(Capability<Pd_session> pd, Cap_quota amount) override {
			warning("Pd_session::transfer_quota not implemented"); }

		Cap_quota cap_quota() const override { return _pd.cap_quota(); }
		Cap_quota used_caps() const override { return _pd.used_caps(); }

		Ram_dataspace_capability alloc(size_t amount, Cache_attribute cached) override {
			return _pd.alloc(amount, cached); }

		void free(Ram_dataspace_capability ds) override { _pd.free(ds); }

		size_t dataspace_size(Ram_dataspace_capability ds) const override {
			return _pd.dataspace_size(ds); }

		void transfer_quota(Pd_session_capability pd, Ram_quota amount) override {
			_pd.transfer_quota(pd, amount); }

		Ram_quota ram_quota() const override { return _pd.ram_quota(); }
		Ram_quota used_ram()  const override { return _pd.used_ram(); }

		Capability<Native_pd> native_pd() override {
			return _pd.native_pd(); }
};

#endif /* _PD_SESSION_COMPONENT_H_ */
