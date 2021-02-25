/*
 * \brief  Shim component to de-couple a child from its parent
 * \author Norman Feske
 * \date   2021-02-25
 */

/*
 * Copyright (C) 2021 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <base/heap.h>
#include <base/child.h>
#include <base/component.h>

namespace Shim {

	struct Main;
	using namespace Genode;
}


class Shim::Main : public Child_policy
{
	private:

		using Parent_service  = Registered<Genode::Parent_service>;
		using Parent_services = Registry<Parent_service>;

		Env &_env;

		Heap _heap { _env.ram(), _env.rm() };

		template <typename QUOTA>
		QUOTA _forwarded_quota(QUOTA total, QUOTA preserved) const
		{
			if (preserved.value > total.value) {
				error("insufficient quota to spawn child "
				      "(have ", total, " need ", preserved, ")");
				throw typename Quota_guard<QUOTA>::Limit_exceeded();
			}

			return QUOTA { total.value - preserved.value };
		}

		Cap_quota const _preserved_caps { Child::env_cap_quota().value + 10 };
		Ram_quota const _preserved_ram  { Child::env_ram_quota().value + 256*1024 };

		Cap_quota const _cap_quota = _forwarded_quota(_env.pd().avail_caps(),
		                                              _preserved_caps);

		Ram_quota const _ram_quota = _forwarded_quota(_env.pd().avail_ram(),
		                                              _preserved_ram);

		Parent_services _parent_services { };

		Child _child { _env.rm(), _env.ep().rpc_ep(), *this };

		Service &_matching_service(Service::Name const &name)
		{
			/* populate session-local parent service registry on demand */
			Service *service_ptr = nullptr;
			_parent_services.for_each([&] (Parent_service &s) {
				if (s.name() == name)
					service_ptr = &s; });

			if (service_ptr)
				return *service_ptr;

			return *new (_heap) Parent_service(_parent_services, _env, name);
		}

		/**
		 * Return sub string of label with leading child name stripped out
		 *
		 * \return character pointer to the scoped part of the label,
		 *         or the original label if the label has no prefix
		 */
		char const *skip_name_prefix(char const *label, size_t const len)
		{
			if (len < 5)
				return label;

			/*
			 * Skip label separator ' -> '
			 */
			if (strcmp(" -> ", label, 4) != 0)
				return label;

			return label + 4;
		}

	public:

		Main(Env &env) : _env(env) { }


		/****************************
		 ** Child-policy interface **
		 ****************************/

		Name name() const override { return ""; }

		Binary_name binary_name() const override { return "binary"; }

		Pd_session           &ref_pd()           override { return _env.pd(); }
		Pd_session_capability ref_pd_cap() const override { return _env.pd_session_cap(); }

		void init(Pd_session &pd, Pd_session_capability pd_cap) override
		{
			pd.ref_account(ref_pd_cap());
			ref_pd().transfer_quota(pd_cap, _cap_quota);
			ref_pd().transfer_quota(pd_cap, _ram_quota);
		}

		Route resolve_session_request(Service::Name const &name,
		                              Session_label const &label,
		                              Session::Diag const  diag) override
		{
			return Route { .service = _matching_service(name),
			               .label   = skip_name_prefix(label.string(),
			                                           label.length()),
			               .diag    = diag };
		}
};


void Component::construct(Genode::Env &env) { static Shim::Main main(env); }
