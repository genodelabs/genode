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
		Region_map        &_core_rm;
		Ram_allocator     &_core_ram;
		Core_account      &_core_account;

		Capability<Cpu_session> _core_cpu_cap;
		Cpu_session            &_core_cpu;

		Cap_quota const _cap_quota;
		Ram_quota const _ram_quota;

		Id_space<Parent::Server> _server_ids { };

		Child _child { _core_rm, _ep, *this };

	public:

		Core_child(Registry<Service> &services, Rpc_entrypoint &ep,
		           Region_map &core_rm, Ram_allocator &core_ram,
		           Core_account &core_account,
		           Cpu_session &core_cpu, Capability<Cpu_session> core_cpu_cap,
		           Cap_quota cap_quota, Ram_quota ram_quota)
		:
			_services(services), _ep(ep), _core_rm(core_rm), _core_ram(core_ram),
			_core_account(core_account),
			_core_cpu_cap(core_cpu_cap), _core_cpu(core_cpu),
			_cap_quota(Child::effective_quota(cap_quota)),
			_ram_quota(Child::effective_quota(ram_quota))
		{ }


		/****************************
		 ** Child-policy interface **
		 ****************************/

		Name name() const override { return "init"; }

		Route resolve_session_request(Service::Name const &name,
		                              Session_label const &label,
		                              Session::Diag const  diag) override
		{
			Service *service = nullptr;
			_services.for_each([&] (Service &s) {
				if (!service && s.name() == name)
					service = &s; });

			if (!service)
				throw Service_denied();

			return Route { .service = *service,
			               .label   = label,
			               .diag    = diag };
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

		void init(Cpu_session &session, Capability<Cpu_session> cap) override
		{
			session.ref_account(_core_cpu_cap);
			_core_cpu.transfer_quota(cap, Cpu_session::quota_lim_upscale(100, 100));
		}

		Ram_allocator &session_md_ram() override { return _core_ram; }

		Pd_account            &ref_account()           override { return _core_account; }
		Capability<Pd_account> ref_account_cap() const override { return _core_account.cap(); }

		size_t session_alloc_batch_size() const override { return 128; }

		Id_space<Parent::Server> &server_id_space() override { return _server_ids; }
};

#endif /* _CORE__INCLUDE__CORE_CHILD_H_ */
