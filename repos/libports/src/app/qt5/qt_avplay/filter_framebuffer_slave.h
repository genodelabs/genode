/*
 * \brief   Filter framebuffer policy
 * \author  Christian Prochaska
 * \date    2012-04-11
 */

/*
 * Copyright (C) 2012-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _FILTER_FRAMEBUFFER_SLAVE_H_
#define _FILTER_FRAMEBUFFER_SLAVE_H_

/* Genode includes */
#include <base/service.h>
#include <os/static_parent_services.h>
#include <os/slave.h>
#include <timer_session/timer_session.h>

/* local includes */
#include "framebuffer_service_factory.h"


class Filter_framebuffer_slave
{
	private:

		class Policy
		:
			private Genode::Static_parent_services<Genode::Cpu_session,
			                                       Genode::Log_session,
			                                       Genode::Pd_session,
			                                       Genode::Rom_session,
			                                       Timer::Session>,
			public Genode::Slave::Policy
		{
			private:

				Framebuffer_service_factory &_framebuffer_service_factory;

			public:

				Policy(Genode::Rpc_entrypoint         &entrypoint,
				       Genode::Region_map             &rm,
				       Genode::Pd_session             &ref_pd,
				       Genode::Pd_session_capability   ref_pd_cap,
				       Name const                     &name,
				       size_t                          caps,
				       size_t                          ram_quota,
				       Framebuffer_service_factory    &framebuffer_service_factory)
				:
					Genode::Slave::Policy(name, name, *this, entrypoint, rm,
					                      ref_pd, ref_pd_cap,
					                      Genode::Cap_quota{caps},
					                      Genode::Ram_quota{ram_quota}),
					_framebuffer_service_factory(framebuffer_service_factory)
				{ }

				Genode::Service &resolve_session_request(Genode::Service::Name       const &service_name,
				                                         Genode::Session_state::Args const &args) override
				{
					if (service_name == "Framebuffer")
						return _framebuffer_service_factory.create(args);

					return Genode::Slave::Policy::resolve_session_request(service_name, args);
				}
		};

		Genode::size_t const   _ep_stack_size = 2*1024*sizeof(Genode::addr_t);
		Genode::Rpc_entrypoint _ep;
		Policy                 _policy;
		Genode::Child          _child;

	public:

		/**
		 * Constructor
		 */
		Filter_framebuffer_slave(Genode::Region_map                &rm,
		                         Genode::Pd_session                &ref_pd,
		                         Genode::Pd_session_capability      ref_pd_cap,
		                         Genode::Slave::Policy::Name const &name,
		                         size_t                             caps,
		                         size_t                             ram_quota,
		                         Framebuffer_service_factory       &framebuffer_service_factory)
		:
			_ep(&ref_pd, _ep_stack_size, "filter_framebuffer_ep"),
			_policy(_ep, rm, ref_pd, ref_pd_cap, name, caps, ram_quota,
			        framebuffer_service_factory),
			_child(rm, _ep, _policy)
		{ }

		Genode::Slave::Policy &policy() { return _policy; }
};

#endif /* _FILTER_FRAMEBUFFER_SLAVE_H_ */
