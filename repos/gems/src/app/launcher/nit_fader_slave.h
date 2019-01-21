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

		struct Policy_base
		{
			Genode::Static_parent_services<Genode::Cpu_session,
			                               Genode::Pd_session,
			                               Genode::Rom_session,
			                               Genode::Log_session,
			                               Timer::Session>
				_parent_services;

			Policy_base(Env &env) : _parent_services(env) { }
		};

		class Policy : Policy_base, public Slave::Policy
		{
			private:

				Genode::Service &_nitpicker_service;

			protected:

				static Name              _name()  { return "nit_fader"; }
				static Genode::Ram_quota _quota() { return { 2*1024*1024 }; }
				static Genode::Cap_quota _caps()  { return { 50 }; }

			public:

				Policy(Env             &env,
				       Rpc_entrypoint  &ep,
				       Genode::Service &nitpicker_service)
				:
					Policy_base(env),
					Genode::Slave::Policy(env, _name(), _name(),
					                      Policy_base::_parent_services,
					                      ep, _caps(), _quota()),
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

				Route resolve_session_request(Genode::Service::Name const &service,
				                              Genode::Session_label const &label) override
				{
					if (service == Nitpicker::Session::service_name())
						return Route { .service = _nitpicker_service,
						               .label   = label,
						               .diag    = Session::Diag() };

					return Genode::Slave::Policy::resolve_session_request(service, label);
				}
		};

		Policy _policy;
		Child  _child;

	public:

		/**
		 * Constructor
		 *
		 * \param ep  entrypoint used for nitpicker child thread
		 */
		Nit_fader_slave(Env             &env,
		                Rpc_entrypoint  &ep,
		                Genode::Service &nitpicker_service)
		:
			_policy(env, ep, nitpicker_service),
			_child(env.rm(), ep, _policy)
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
