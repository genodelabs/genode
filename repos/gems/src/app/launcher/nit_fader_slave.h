/*
 * \brief  Slave used for toggling the visibility of a nitpicker session
 * \author Norman Feske
 * \date   2014-09-30
 */

/*
 * Copyright (C) 2014-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _NIT_FADER_SLAVE_H_
#define _NIT_FADER_SLAVE_H_

/* Genode includes */
#include <os/static_parent_services.h>
#include <os/slave.h>
#include <nitpicker_session/nitpicker_session.h>

/* local includes */
#include <types.h>

namespace Launcher { class Nit_fader_slave; }


class Launcher::Nit_fader_slave
{
	private:

		class Policy
		:
			private Genode::Static_parent_services<Genode::Ram_session,
			                                       Genode::Cpu_session,
			                                       Genode::Pd_session,
			                                       Genode::Rom_session,
			                                       Genode::Log_session,
			                                       Timer::Session>,
			public Slave::Policy
		{
			private:

				Genode::Service &_nitpicker_service;

			protected:

				static Name              _name()  { return "nit_fader"; }
				static Genode::Ram_quota _quota() { return { 2*1024*1024 }; }
				static Genode::Cap_quota _caps()  { return { 25 }; }

			public:

				Policy(Rpc_entrypoint        &ep,
				       Region_map            &rm,
				       Pd_session            &ref_pd,
				       Pd_session_capability  ref_pd_cap,
				       Genode::Service       &nitpicker_service)
				:
					Genode::Slave::Policy(_name(), _name(), *this, ep, rm,
					                      ref_pd,  ref_pd_cap,  _caps(), _quota()),
					_nitpicker_service(nitpicker_service)
				{
					visible(false);
				}

				void visible(bool visible)
				{
					char config[256];
					snprintf(config, sizeof(config),
					         "<config alpha=\"%d\" />", visible ? 255 : 0);
					configure(config, strlen(config) + 1);
				}

				Genode::Service &resolve_session_request(Genode::Service::Name const &service,
				                                         Genode::Session_state::Args const &args) override
				{
					if (service == Nitpicker::Session::service_name())
						return _nitpicker_service;

					return Genode::Slave::Policy::resolve_session_request(service, args);
				}
		};

		Policy _policy;
		Child  _child;

	public:

		/**
		 * Constructor
		 *
		 * \param ep       entrypoint used for nitpicker child thread
		 * \param ref_ram  RAM session used to allocate the configuration
		 *                 dataspace and child memory
		 */
		Nit_fader_slave(Rpc_entrypoint        &ep,
		                Genode::Region_map    &rm,
		                Pd_session            &ref_pd,
		                Pd_session_capability  ref_pd_cap,
		                Genode::Service       &nitpicker_service)
		:
			_policy(ep, rm, ref_pd, ref_pd_cap, nitpicker_service),
			_child(rm, ep, _policy)
		{
			visible(false);
		}

		Genode::Slave::Policy &policy() { return _policy; }

		void visible(bool visible)
		{
			_policy.visible(visible);
		}
};

#endif /* _NIT_FADER_SLAVE_H_ */
