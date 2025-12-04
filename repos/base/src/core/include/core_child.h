/*
 * \brief  Child policy for the init component
 * \author Norman Feske
 * \date   2024-12-17
 */

/*
 * Copyright (C) 2016-2024 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _CORE__INCLUDE__CORE_CHILD_H_
#define _CORE__INCLUDE__CORE_CHILD_H_

/* Genode includes */
#include <base/service.h>
#include <base/child.h>

/* core includes */
#include <core_account.h>

namespace Core { class Core_child; }


class Core::Core_child : public Child_policy
{
	private:

		Registry<Service> &_services;
		Rpc_entrypoint    &_ep;
		Local_rm          &_local_rm;
		Ram_allocator     &_core_ram;
		Core_account      &_core_account;

		Cap_quota const _cap_quota;
		Ram_quota const _ram_quota;

		Id_space<Parent::Server> _server_ids { };

		Child _child { _local_rm, _ep, *this };

	public:

		Core_child(Registry<Service> &services, Rpc_entrypoint &ep,
		           Local_rm &local_rm, Ram_allocator &core_ram,
		           Core_account &core_account, Cap_quota cap_quota, Ram_quota ram_quota)
		:
			_services(services), _ep(ep), _local_rm(local_rm), _core_ram(core_ram),
			_core_account(core_account),
			_cap_quota(Child::effective_quota(cap_quota)),
			_ram_quota(Child::effective_quota(ram_quota))
		{ }


		/****************************
		 ** Child-policy interface **
		 ****************************/

		Name name() const override { return "init"; }

		void _with_route(Service::Name     const &name,
		                 Session_label     const &label,
		                 With_route::Ft    const &fn,
		                 With_no_route::Ft const &denied_fn) override
		{
			Service *service = nullptr;
			_services.for_each([&] (Service &s) {
				if (!service && s.name() == name)
					service = &s; });

			if (service)
				fn(Route { .service = *service, .label = label });
			else
				denied_fn();
		}

		void init(Pd_session &, Capability<Pd_session> cap) override
		{
			_ep.apply(cap, [&] (Pd_session_component *pd_ptr) {
				if (pd_ptr)
					pd_ptr->ref_accounts(_core_account.ram_account,
					                     _core_account.cap_account); });

			_core_account.transfer_quota(cap, _cap_quota);
			_core_account.transfer_quota(cap, _ram_quota);
		}

		Ram_allocator &session_md_ram() override { return _core_ram; }

		Pd_account            &ref_account()           override { return _core_account; }
		Capability<Pd_account> ref_account_cap() const override { return _core_account.cap(); }

		size_t session_alloc_batch_size() const override { return 128; }

		Id_space<Parent::Server> &server_id_space() override { return _server_ids; }
};

#endif /* _CORE__INCLUDE__CORE_CHILD_H_ */
