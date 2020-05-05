/*
 * \brief  Core-specific instance of the PD session interface
 * \author Christian Helmuth
 * \author Stefan Kalkowski
 * \author Norman Feske
 * \date   2006-07-17
 */

/*
 * Copyright (C) 2006-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _CORE__INCLUDE__PD_SESSION_COMPONENT_H_
#define _CORE__INCLUDE__PD_SESSION_COMPONENT_H_

/* Genode includes */
#include <util/reconstructible.h>
#include <base/session_object.h>
#include <base/registry.h>
#include <base/heap.h>
#include <pd_session/pd_session.h>
#include <util/arg_string.h>

/* base-internal includes */
#include <base/internal/stack_area.h>

/* core includes */
#include <constrained_core_ram.h>
#include <platform_pd.h>
#include <signal_broker.h>
#include <rpc_cap_factory.h>
#include <ram_dataspace_factory.h>
#include <native_pd_component.h>
#include <region_map_component.h>
#include <platform_generic.h>
#include <account.h>

namespace Genode { class Pd_session_component; }


class Genode::Pd_session_component : public Session_object<Pd_session>
{
	private:

		Rpc_entrypoint            &_ep;
		Constrained_ram_allocator  _constrained_md_ram_alloc;
		Constrained_core_ram       _constrained_core_ram_alloc;
		Sliced_heap                _sliced_heap;
		Capability<Parent>         _parent { };
		Ram_dataspace_factory      _ram_ds_factory;
		Signal_broker              _signal_broker;
		Rpc_cap_factory            _rpc_cap_factory;
		Native_pd_component        _native_pd;

		Constructible<Platform_pd> _pd { };

		Constructible<Account<Cap_quota> > _cap_account { };
		Constructible<Account<Ram_quota> > _ram_account { };

		Region_map_component _address_space;
		Region_map_component _stack_area;
		Region_map_component _linker_area;

		friend class Native_pd_component;


		/*****************************************
		 ** Utilities for capability accounting **
		 *****************************************/

		enum Cap_type { RPC_CAP, DS_CAP, SIG_SOURCE_CAP, SIG_CONTEXT_CAP, IGN_CAP };

		char const *_name(Cap_type type)
		{
			switch (type) {
			case RPC_CAP:         return "RPC";
			case DS_CAP:          return "dataspace";
			case SIG_SOURCE_CAP:  return "signal-source";
			case SIG_CONTEXT_CAP: return "signal-context";
			default:              return "";
			}
		}

		/*
		 * \throw Out_of_caps
		 */
		void _consume_cap(Cap_type type)
		{
			try { withdraw(Cap_quota{1}); }
			catch (Out_of_caps) {
				diag("out of caps while consuming ", _name(type), " cap "
				     "(", _cap_account, ")");
				throw;
			}
			diag("consumed ", _name(type), " cap (", _cap_account, ")");
		}

		void _released_cap_silent() { replenish(Cap_quota{1}); }

		void _released_cap(Cap_type type)
		{
			_released_cap_silent();
			diag("released ", _name(type), " cap (", _cap_account, ")");
		}

	public:

		typedef Ram_dataspace_factory::Phys_range Phys_range;
		typedef Ram_dataspace_factory::Virt_range Virt_range;

		/**
		 * Constructor
		 */
		Pd_session_component(Rpc_entrypoint   &ep,
		                     Rpc_entrypoint   &signal_ep,
		                     Resources         resources,
		                     Label      const &label,
		                     Diag              diag,
		                     Range_allocator  &phys_alloc,
		                     Phys_range        phys_range,
		                     Virt_range        virt_range,
		                     Region_map       &local_rm,
		                     Pager_entrypoint &pager_ep,
		                     char const       *args,
		                     Range_allocator  &core_mem)
		:
			Session_object(ep, resources, label, diag),
			_ep(ep),
			_constrained_md_ram_alloc(*this, _ram_quota_guard(), _cap_quota_guard()),
			_constrained_core_ram_alloc(_ram_quota_guard(), _cap_quota_guard(), core_mem),
			_sliced_heap(_constrained_md_ram_alloc, local_rm),
			_ram_ds_factory(ep, phys_alloc, phys_range, local_rm,
			                _constrained_core_ram_alloc),
			_signal_broker(_sliced_heap, signal_ep, signal_ep),
			_rpc_cap_factory(_sliced_heap),
			_native_pd(*this, args),
			_address_space(ep, _sliced_heap, pager_ep,
			               virt_range.start, virt_range.size, diag),
			_stack_area (ep, _sliced_heap, pager_ep, 0, stack_area_virtual_size(), diag),
			_linker_area(ep, _sliced_heap, pager_ep, 0, LINKER_AREA_SIZE, diag)
		{
			if (platform().core_needs_platform_pd() || label != "core") {
				_pd.construct(_sliced_heap, _label.string());
				_address_space.address_space(&*_pd);
			}
		}

		/**
		 * Initialize cap and RAM accounts without providing a reference account
		 *
		 * This method is solely used to set up the initial PD within core. The
		 * accounts of regular PD sessions are initialized via 'ref_account'.
		 */
		void init_cap_and_ram_accounts()
		{
			_cap_account.construct(_cap_quota_guard(), _label);
			_ram_account.construct(_ram_quota_guard(), _label);
		}

		/**
		 * Associate thread with PD
		 *
		 * \return true on success
		 *
		 * This function may fail for platform-specific reasons such as a
		 * limit on the number of threads per protection domain or a limited
		 * thread ID namespace.
		 */
		bool bind_thread(Platform_thread &thread)
		{
			return _pd->bind_thread(thread);
		}

		Region_map_component &address_space_region_map()
		{
			return _address_space;
		}

		void assign_parent(Capability<Parent> parent) override
		{
			_parent = parent;
			_pd->assign_parent(parent);
		}

		bool assign_pci(addr_t, uint16_t) override;

		void map(addr_t, addr_t) override;

		/****************
		 ** Signalling **
		 ****************/

		Signal_source_capability alloc_signal_source() override
		{
			_consume_cap(SIG_SOURCE_CAP);
			try { return _signal_broker.alloc_signal_source(); }
			catch (Genode::Allocator::Out_of_memory) {
				_released_cap_silent();
				throw Out_of_ram();
			}
		}

		void free_signal_source(Signal_source_capability sig_rec_cap) override
		{
			if (sig_rec_cap.valid()) {
				_signal_broker.free_signal_source(sig_rec_cap);
				_released_cap(SIG_SOURCE_CAP);
			}
		}

		Signal_context_capability
		alloc_context(Signal_source_capability sig_rec_cap, unsigned long imprint) override
		{
			Cap_quota_guard::Reservation cap_costs(_cap_quota_guard(), Cap_quota{1});
			try {
				/* may throw 'Out_of_ram' or 'Invalid_signal_source' */
				Signal_context_capability cap =
					_signal_broker.alloc_context(sig_rec_cap, imprint);

				cap_costs.acknowledge();
				diag("consumed signal-context cap (", _cap_account, ")");
				return cap;
			}
			catch (Signal_broker::Invalid_signal_source) {
				throw Pd_session::Invalid_signal_source(); }
		}

		void free_context(Signal_context_capability cap) override
		{
			_signal_broker.free_context(cap);
			_released_cap(SIG_CONTEXT_CAP);
		}

		void submit(Signal_context_capability cap, unsigned n) override {
			_signal_broker.submit(cap, n); }


		/*******************************
		 ** RPC capability allocation **
		 *******************************/

		Native_capability alloc_rpc_cap(Native_capability ep) override
		{
			/* may throw 'Out_of_caps' */
			_consume_cap(RPC_CAP);

			/* may throw 'Out_of_ram' */
			try {  return _rpc_cap_factory.alloc(ep); }
			catch (...) { _released_cap_silent(); throw; }
		}

		void free_rpc_cap(Native_capability cap) override
		{
			_rpc_cap_factory.free(cap);
			_released_cap(RPC_CAP);
		}


		/******************************
		 ** Address-space management **
		 ******************************/

		Capability<Region_map> address_space() override {
			return _address_space.cap(); }

		Capability<Region_map> stack_area() override {
			return _stack_area.cap(); }

		Capability<Region_map> linker_area() override {
			return _linker_area.cap(); }


		/***********************************************
		 ** Capability and RAM trading and accounting **
		 ***********************************************/

		void ref_account(Capability<Pd_session>) override;

		void transfer_quota(Capability<Pd_session>, Cap_quota) override;
		void transfer_quota(Capability<Pd_session>, Ram_quota) override;

		Cap_quota cap_quota() const override
		{
			return _cap_account.constructed() ? _cap_account->limit() : Cap_quota { 0 };
		}

		Cap_quota used_caps() const override
		{
			return _cap_account.constructed() ? _cap_account->used() : Cap_quota { 0 };
		}

		Ram_quota ram_quota() const override
		{
			return _ram_account.constructed() ? _ram_account->limit() : Ram_quota { 0 };
		}

		Ram_quota used_ram() const override
		{
			return _ram_account.constructed() ? _ram_account->used() : Ram_quota { 0 };
		}


		/***********************************
		 ** RAM allocation and accounting **
		 ***********************************/

		Ram_dataspace_capability alloc(size_t, Cache_attribute) override;

		void free(Ram_dataspace_capability) override;

		size_t dataspace_size(Ram_dataspace_capability) const override;


		/*******************************************
		 ** Platform-specific interface extension **
		 *******************************************/

		Capability<Native_pd> native_pd() override { return _native_pd.cap(); }
};

#endif /* _CORE__INCLUDE__PD_SESSION_COMPONENT_H_ */
