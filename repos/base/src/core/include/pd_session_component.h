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
#include <base/allocator_guard.h>
#include <base/session_object.h>
#include <base/registry.h>
#include <base/heap.h>
#include <pd_session/pd_session.h>
#include <util/arg_string.h>

/* base-internal includes */
#include <base/internal/stack_area.h>

/* core includes */
#include <platform_pd.h>
#include <signal_broker.h>
#include <rpc_cap_factory.h>
#include <native_pd_component.h>
#include <region_map_component.h>
#include <platform_generic.h>
#include <account.h>

namespace Genode { class Pd_session_component; }


class Genode::Pd_session_component : public Session_object<Pd_session>
{
	private:

		Constrained_ram_allocator _constrained_md_ram_alloc;
		Sliced_heap               _sliced_heap;
		Platform_pd               _pd { &_sliced_heap, _label.string() };
		Capability<Parent>        _parent;
		Rpc_entrypoint           &_ep;
		Pager_entrypoint         &_pager_ep;
		Signal_broker             _signal_broker { _sliced_heap, _ep, _ep };
		Rpc_cap_factory           _rpc_cap_factory { _sliced_heap };
		Native_pd_component       _native_pd;

		Region_map_component _address_space;
		Region_map_component _stack_area;
		Region_map_component _linker_area;

		Constructible<Account<Cap_quota> > _cap_account;

		friend class Native_pd_component;

		enum Cap_type { RPC_CAP, SIG_SOURCE_CAP, SIG_CONTEXT_CAP, IGN_CAP };

		char const *_name(Cap_type type)
		{
			switch (type) {
			case RPC_CAP:         return "RPC";
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
			try { Cap_quota_guard::withdraw(Cap_quota{1}); }
			catch (Out_of_caps) {
				diag("out of caps while consuming ", _name(type), " cap "
				     "(", _cap_account, ")");
				throw;
			}
			diag("consumed ", _name(type), " cap (", _cap_account, ")");
		}

		void _released_cap_silent() { Cap_quota_guard::replenish(Cap_quota{1}); }

		void _released_cap(Cap_type type)
		{
			_released_cap_silent();
			diag("released ", _name(type), " cap (", _cap_account, ")");
		}

	public:

		/**
		 * Constructor
		 *
		 * \param ep          entrypoint holding signal-receiver component
		 *                    objects, signal contexts, thread objects
		 * \param ram_alloc   backing store for dynamically allocated
		 *                    session meta data
		 */
		Pd_session_component(Rpc_entrypoint   &ep,
		                     Resources         resources,
		                     Label      const &label,
		                     Diag              diag,
		                     Ram_allocator    &ram_alloc,
		                     Region_map       &local_rm,
		                     Pager_entrypoint &pager_ep,
		                     char const       *args)
		:
			Session_object(ep, resources, label, diag),
			_constrained_md_ram_alloc(ram_alloc, *this, *this),
			_sliced_heap(_constrained_md_ram_alloc, local_rm),
			_ep(ep), _pager_ep(pager_ep),
			_rpc_cap_factory(_sliced_heap),
			_native_pd(*this, args),
			_address_space(ep, _sliced_heap, pager_ep,
			               platform()->vm_start(), platform()->vm_size()),
			_stack_area (_ep, _sliced_heap, pager_ep, 0, stack_area_virtual_size()),
			_linker_area(_ep, _sliced_heap, pager_ep, 0, LINKER_AREA_SIZE)
		{ }

		/**
		 * Initialize cap account without providing a reference account
		 *
		 * This method is solely used to set up the initial PD session within
		 * core. The cap accounts of regular PD session are initialized via
		 * 'ref_account'.
		 */
		void init_cap_account() { _cap_account.construct(*this, _label); }

		/**
		 * Session_object interface
		 */
		void session_quota_upgraded() override;

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
			return _pd.bind_thread(&thread);
		}

		Region_map_component &address_space_region_map()
		{
			return _address_space;
		}


		/**************************
		 ** PD session interface **
		 **************************/

		void assign_parent(Capability<Parent> parent) override
		{
			_parent = parent;
			_pd.assign_parent(parent);
		}

		bool assign_pci(addr_t, uint16_t) override;

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
			_signal_broker.free_signal_source(sig_rec_cap);
			_released_cap(SIG_SOURCE_CAP);
		}

		Signal_context_capability
		alloc_context(Signal_source_capability sig_rec_cap, unsigned long imprint) override
		{
			Cap_quota_guard::Reservation cap_costs(*this, Cap_quota{1});
			try {
				Signal_context_capability cap =
					_signal_broker.alloc_context(sig_rec_cap, imprint);

				cap_costs.acknowledge();
				diag("consumed signal-context cap (", _cap_account, ")");
				return cap;
			}
			catch (Genode::Allocator::Out_of_memory) {
				throw Out_of_ram(); }
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

		/*
		 * \throw Out_of_caps  by '_consume_cap'
		 * \throw Out_of_ram   by '_rpc_cap_factory.alloc'
		 */
		Native_capability alloc_rpc_cap(Native_capability ep) override
		{
			_consume_cap(RPC_CAP);
			return _rpc_cap_factory.alloc(ep);
		}

		void free_rpc_cap(Native_capability cap) override
		{
			_rpc_cap_factory.free(cap);
			_released_cap(RPC_CAP);
		}

		Capability<Region_map> address_space() {
			return _address_space.cap(); }

		Capability<Region_map> stack_area() {
			return _stack_area.cap(); }

		Capability<Region_map> linker_area() {
			return _linker_area.cap(); }

		void ref_account(Capability<Pd_session> pd_cap) override
		{
			/* the reference account can be defined only once */
			if (_cap_account.constructed())
				return;

			if (this->cap() == pd_cap)
				return;

			_ep.apply(pd_cap, [&] (Pd_session_component *pd) {

				if (!pd || !pd->_cap_account.constructed()) {
					error("invalid PD session specified as ref account");
					throw Invalid_session();
				}

				_cap_account.construct(*this, _label, *pd->_cap_account);
			});
		}

		void transfer_quota(Capability<Pd_session> pd_cap, Cap_quota amount) override
		{
			/* the reference account can be defined only once */
			if (!_cap_account.constructed())
				throw Undefined_ref_account();

			if (this->cap() == pd_cap)
				return;

			_ep.apply(pd_cap, [&] (Pd_session_component *pd) {

				if (!pd || !pd->_cap_account.constructed())
					throw Invalid_session();

				try {
					_cap_account->transfer_quota(*pd->_cap_account, amount);
					diag("transferred ", amount, " caps "
					     "to '", pd->_cap_account->label(), "' (", _cap_account, ")");
				}
				catch (Account<Cap_quota>::Unrelated_account) {
					warning("attempt to transfer cap quota to unrelated PD session");
					throw Invalid_session(); }
				catch (Account<Cap_quota>::Limit_exceeded) {
					warning("cap limit (", *_cap_account, ") exceeded "
					        "during transfer_quota(", amount, ")");
					throw Out_of_caps(); }
			});
		}

		Cap_quota cap_quota() const
		{
			return _cap_account.constructed() ? _cap_account->limit() : Cap_quota { 0 };
		}

		Cap_quota used_caps() const
		{
			return _cap_account.constructed() ? _cap_account->used() : Cap_quota { 0 };
		}

		Capability<Native_pd> native_pd() { return _native_pd.cap(); }
};

#endif /* _CORE__INCLUDE__PD_SESSION_COMPONENT_H_ */
