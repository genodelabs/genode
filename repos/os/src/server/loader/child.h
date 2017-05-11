/*
 * \brief  Loader child interface
 * \author Christian Prochaska
 * \author Norman Feske
 * \date   2009-10-05
 */

/*
 * Copyright (C) 2009-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _CHILD_H_
#define _CHILD_H_

/* Genode includes */
#include <base/child.h>
#include <util/arg_string.h>
#include <init/child_policy.h>
#include <cpu_session/connection.h>
#include <pd_session/connection.h>
#include <region_map/client.h>

namespace Loader {

	using namespace Genode;

	typedef Registered<Parent_service> Parent_service;
	typedef Registry<Parent_service>   Parent_services;

	class Child;
}


class Loader::Child : public Child_policy
{
	private:

		Env       &_env;
		Allocator &_alloc;

		Session_label const _label;
		Name          const _binary_name;

		Cap_quota const _cap_quota;
		Ram_quota const _ram_quota;

		Parent_services &_parent_services;

		Service &_local_nitpicker_service;
		Service &_local_rom_service;
		Service &_local_cpu_service;
		Service &_local_pd_service;

		Genode::Child _child;

	public:

		Child(Env                       &env,
		      Allocator                 &alloc,
		      Name                const &binary_name,
		      Session_label       const &label,
		      Cap_quota                  cap_quota,
		      Ram_quota                  ram_quota,
		      Parent_services           &parent_services,
		      Service                   &local_rom_service,
		      Service                   &local_cpu_service,
		      Service                   &local_pd_service,
		      Service                   &local_nitpicker_service,
		      Signal_context_capability fault_sigh)
		:
			_env(env),
			_alloc(alloc),
			_label(label),
			_binary_name(binary_name),
			_cap_quota(Genode::Child::effective_quota(cap_quota)),
			_ram_quota(Genode::Child::effective_quota(ram_quota)),
			_parent_services(parent_services),
			_local_nitpicker_service(local_nitpicker_service),
			_local_rom_service(local_rom_service),
			_local_cpu_service(local_cpu_service),
			_local_pd_service(local_pd_service),
			_child(_env.rm(), _env.ep().rpc_ep(), *this)
		{ }

		~Child() { }


		/****************************
		 ** Child-policy interface **
		 ****************************/

		Name name() const override { return _label; }

		Binary_name binary_name() const override { return _binary_name; }

		Pd_session           &ref_pd()           override { return _env.pd(); }
		Pd_session_capability ref_pd_cap() const override { return _env.pd_session_cap(); }

		void init(Pd_session &pd, Pd_session_capability pd_cap) override
		{
			pd.ref_account(ref_pd_cap());
			ref_pd().transfer_quota(pd_cap, _cap_quota);
			ref_pd().transfer_quota(pd_cap, _ram_quota);
		}

		Service &resolve_session_request(Service::Name const &name,
		                                 Session_state::Args const &args) override
		{
			if (name == "Nitpicker") return _local_nitpicker_service;
			if (name == "ROM")       return _local_rom_service;
			if (name == "CPU")       return _local_cpu_service;
			if (name == "PD")        return _local_pd_service;

			/* populate session-local parent service registry on demand */
			Service *service = nullptr;
			_parent_services.for_each([&] (Parent_service &s) {
				if (s.name() == name)
					service = &s; });

			if (service)
				return *service;

			return *new (_alloc) Parent_service(_parent_services, name);
		}
};

#endif /* _CHILD_H_ */
