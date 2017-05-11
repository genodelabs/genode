/*
 * \brief  Core-specific instance of the RAM session interface
 * \author Norman Feske
 * \date   2006-06-19
 */

/*
 * Copyright (C) 2006-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _CORE__INCLUDE__RAM_SESSION_COMPONENT_H_
#define _CORE__INCLUDE__RAM_SESSION_COMPONENT_H_

/* Genode includes */
#include <base/heap.h>
#include <base/session_object.h>

/* core includes */
#include <ram_dataspace_factory.h>
#include <account.h>

namespace Genode { class Ram_session_component; }


class Genode::Ram_session_component : public Session_object<Ram_session>
{
	private:

		Rpc_entrypoint &_ep;

		Constrained_ram_allocator _constrained_md_ram_alloc;

		Sliced_heap _sliced_heap;

		Constructible<Account<Ram_quota> > _ram_account;

		Ram_dataspace_factory _ram_ds_factory;

	public:

		typedef Ram_dataspace_factory::Phys_range Phys_range;

		Ram_session_component(Rpc_entrypoint      &ep,
		                      Resources            resources,
		                      Session_label const &label,
		                      Diag                 diag,
		                      Range_allocator     &phys_alloc,
		                      Region_map          &local_rm,
		                      Phys_range           phys_range);

		/**
		 * Initialize RAM account without providing a reference account
		 *
		 * This method is solely used to set up the initial RAM session within
		 * core. The RAM accounts of regular RAM session are initialized via
		 * 'ref_account'.
		 */
		void init_ram_account() { _ram_account.construct(*this, _label); }


		/*****************************
		 ** Ram_allocator interface **
		 *****************************/

		Ram_dataspace_capability alloc(size_t, Cache_attribute) override;

		void free(Ram_dataspace_capability) override;

		size_t dataspace_size(Ram_dataspace_capability) const override;


		/***************************
		 ** RAM Session interface **
		 ***************************/

		void ref_account(Ram_session_capability) override;

		void transfer_quota(Ram_session_capability, Ram_quota) override;

		Ram_quota ram_quota() const override
		{
			return _ram_account.constructed() ? _ram_account->limit() : Ram_quota { 0 };
		}

		Ram_quota used_ram() const override
		{
			return _ram_account.constructed() ? _ram_account->used() : Ram_quota { 0 };
		}
};

#endif /* _CORE__INCLUDE__RAM_SESSION_COMPONENT_H_ */
