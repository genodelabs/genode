/*
 * \brief  PD service used by Noux processes
 * \author Norman Feske
 * \date   2016-04-20
 *
 * The custom implementation of the PD session interface provides a pool of
 * RAM shared by Noux and all Noux processes. The use of a shared pool
 * alleviates the need to assign RAM quota to individual Noux processes.
 *
 * Furthermore, the custom implementation is needed to get hold of the RAM
 * dataspaces allocated by each Noux process. When forking a process, the
 * acquired information (in the form of 'Ram_dataspace_info' objects) is used
 * to create a shadow copy of the forking address space.
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
#include <dataspace_registry.h>

namespace Noux {
	struct Ram_dataspace_info;
	struct Pd_session_component;
	using namespace Genode;
}


struct Noux::Ram_dataspace_info : Dataspace_info,
                                  List<Ram_dataspace_info>::Element
{
	Ram_dataspace_info(Ram_dataspace_capability ds_cap)
	: Dataspace_info(ds_cap) { }

	Dataspace_capability fork(Ram_allocator      &ram,
	                          Region_map         &local_rm,
	                          Allocator          &alloc,
	                          Dataspace_registry &ds_registry,
	                          Rpc_entrypoint     &) override
	{
		size_t const size = Dataspace_client(ds_cap()).size();
		Ram_dataspace_capability dst_ds_cap;

		try {
			dst_ds_cap = ram.alloc(size);

			Attached_dataspace src_ds(local_rm, ds_cap());
			Attached_dataspace dst_ds(local_rm, dst_ds_cap);
			memcpy(dst_ds.local_addr<char>(), src_ds.local_addr<char>(), size);

			ds_registry.insert(new (alloc) Ram_dataspace_info(dst_ds_cap));
			return dst_ds_cap;

		} catch (...) {
			error("fork of RAM dataspace failed");

			if (dst_ds_cap.valid())
				ram.free(dst_ds_cap);

			return Dataspace_capability();
		}
	}

	void poke(Region_map &rm, addr_t dst_offset, char const *src, size_t len) override
	{
		if (!src) return;

		if ((dst_offset >= size()) || (dst_offset + len > size())) {
			error("illegal attemt to write beyond dataspace boundary");
			return;
		}

		try {
			Attached_dataspace ds(rm, ds_cap());
			memcpy(ds.local_addr<char>() + dst_offset, src, len);
		} catch (...) { warning("poke: failed to attach RAM dataspace"); }
	}
};


class Noux::Pd_session_component : public Rpc_object<Pd_session>
{
	private:

		Rpc_entrypoint &_ep;

		Pd_connection _pd;

		Pd_session &_ref_pd;

		Region_map_component _address_space;
		Region_map_component _stack_area;
		Region_map_component _linker_area;

		Allocator &_alloc;

		Ram_allocator &_ram;

		Ram_quota _used_ram_quota { 0 };

		List<Ram_dataspace_info> _ds_list;

		Dataspace_registry &_ds_registry;

		template <typename FUNC>
		auto _with_automatic_cap_upgrade(FUNC func) -> decltype(func())
		{
			Cap_quota upgrade { 10 };
			enum { NUM_ATTEMPTS = 3 };
			return retry<Out_of_caps>(
				[&] () { return func(); },
				[&] () { _ref_pd.transfer_quota(_pd, upgrade); },
				NUM_ATTEMPTS);
		}

	public:

		/**
		 * Constructor
		 */
		Pd_session_component(Allocator &alloc, Env &env, Rpc_entrypoint &ep,
		                     Child_policy::Name const &name,
		                     Dataspace_registry &ds_registry)
		:
			_ep(ep), _pd(env, name.string()), _ref_pd(env.pd()),
			_address_space(alloc, _ep, ds_registry, _pd, _pd.address_space()),
			_stack_area   (alloc, _ep, ds_registry, _pd, _pd.stack_area()),
			_linker_area  (alloc, _ep, ds_registry, _pd, _pd.linker_area()),
			_alloc(alloc), _ram(env.ram()), _ds_registry(ds_registry)
		{
			_ep.manage(this);

			/*
			 * Equip the PD with an initial cap quota that suffices in the
			 * common case. Further capabilities are provisioned on demand
			 * via '_with_automatic_cap_upgrade'.
			 */
			_pd.ref_account(env.pd_session_cap());
			_ref_pd.transfer_quota(_pd, Cap_quota{10});
		}

		~Pd_session_component()
		{
			_ep.dissolve(this);

			Ram_dataspace_info *info = 0;
			while ((info = _ds_list.first()))
				free(static_cap_cast<Ram_dataspace>(info->ds_cap()));
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

		void replay(Pd_session_component &dst_pd,
		            Region_map           &local_rm,
		            Allocator            &alloc,
		            Dataspace_registry   &ds_registry,
		            Rpc_entrypoint       &ep)
		{
			/* replay region map into new protection domain */
			_stack_area   .replay(dst_pd, dst_pd.stack_area_region_map(),    local_rm, alloc, ds_registry, ep);
			_linker_area  .replay(dst_pd, dst_pd.linker_area_region_map(),   local_rm, alloc, ds_registry, ep);
			_address_space.replay(dst_pd, dst_pd.address_space_region_map(), local_rm, alloc, ds_registry, ep);

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

		Signal_source_capability alloc_signal_source() override
		{
			return _with_automatic_cap_upgrade([&] () {
				return _pd.alloc_signal_source(); });
		}

		void free_signal_source(Signal_source_capability cap) override {
			_pd.free_signal_source(cap); }

		Capability<Signal_context> alloc_context(Signal_source_capability source,
		                                         unsigned long imprint) override
		{
			return _with_automatic_cap_upgrade([&] () {
				return _pd.alloc_context(source, imprint); });
		}

		void free_context(Capability<Signal_context> cap) override {
			_pd.free_context(cap); }

		void submit(Capability<Signal_context> context, unsigned cnt) override {
			_pd.submit(context, cnt); }

		Native_capability alloc_rpc_cap(Native_capability ep) override
		{
			return _with_automatic_cap_upgrade([&] () {
				return _pd.alloc_rpc_cap(ep); });
		}

		void free_rpc_cap(Native_capability cap) override {
			_pd.free_rpc_cap(cap); }

		Capability<Region_map> address_space() override {
			return _address_space.Rpc_object<Region_map>::cap(); }

		Capability<Region_map> stack_area() override {
			return _stack_area.Rpc_object<Region_map>::cap(); }

		Capability<Region_map> linker_area() override {
			return _linker_area.Rpc_object<Region_map>::cap(); }

		void ref_account(Capability<Pd_session> pd) override { }

		void transfer_quota(Capability<Pd_session> pd, Cap_quota amount) override { }

		Cap_quota cap_quota() const { return _pd.cap_quota(); }
		Cap_quota used_caps() const { return _pd.used_caps(); }

		Ram_dataspace_capability alloc(size_t size, Cache_attribute cached) override
		{
			Ram_dataspace_capability ds_cap = _ram.alloc(size, cached);

			Ram_dataspace_info *ds_info = new (_alloc) Ram_dataspace_info(ds_cap);

			_ds_registry.insert(ds_info);
			_ds_list.insert(ds_info);

			_used_ram_quota = Ram_quota { _used_ram_quota.value + size };

			return ds_cap;
		}

		void free(Ram_dataspace_capability ds_cap) override
		{
			Ram_dataspace_info *ds_info;

			auto lambda = [&] (Ram_dataspace_info *rdi) {
				ds_info = rdi;

				if (!ds_info) {
					error("RAM free: dataspace lookup failed");
					return;
				}

				size_t const ds_size = rdi->size();

				_ds_registry.remove(ds_info);
				ds_info->dissolve_users();
				_ds_list.remove(ds_info);
				_ram.free(ds_cap);

				_used_ram_quota = Ram_quota { _used_ram_quota.value - ds_size };
			};
			_ds_registry.apply(ds_cap, lambda);
			destroy(_alloc, ds_info);
		}

		size_t dataspace_size(Ram_dataspace_capability ds_cap) const override
		{
			size_t result = 0;
			_ds_registry.apply(ds_cap, [&] (Ram_dataspace_info *rdi) {
				if (rdi)
					result = rdi->size(); });
			return result;
		}

		void transfer_quota(Pd_session_capability, Ram_quota) override { }
		Ram_quota ram_quota() const override { return _pd.ram_quota(); }
		Ram_quota used_ram()  const override { return Ram_quota{_used_ram_quota}; }

		Capability<Native_pd> native_pd() override {
			return _pd.native_pd(); }
};

#endif /* _NOUX__PD_SESSION_COMPONENT_H_ */
