/*
 * \brief  Slave used for toggling the visibility of a nitpicker session
 * \author Norman Feske
 * \date   2014-09-30
 */

/*
 * Copyright (C) 2014 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _NIT_FADER_SLAVE_H_
#define _NIT_FADER_SLAVE_H_

/* Genode includes */
#include <os/slave.h>
#include <nitpicker_session/nitpicker_session.h>

/* local includes */
#include <types.h>

namespace Launcher { class Nit_fader_slave; }


class Launcher::Nit_fader_slave
{
	private:

		class Policy : public Slave_policy
		{
			private:

				Genode::Service &_nitpicker_service;
				Lock     mutable _nitpicker_root_lock { Lock::LOCKED };
				Capability<Root> _nitpicker_root_cap;

			protected:

				char const **_permitted_services() const
				{
					static char const *permitted_services[] = {
						"LOG", "RM", "Timer", 0 };

					return permitted_services;
				};

			public:

				Policy(Rpc_entrypoint  &entrypoint,
				       Ram_session     &ram,
				       Genode::Service &nitpicker_service)
				:
					Slave_policy("nit_fader", entrypoint, &ram),
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

				bool announce_service(const char     *service_name,
				                      Root_capability root,
				                      Allocator      *,
				                      Genode::Server *)
				{
					if (strcmp(service_name, "Nitpicker") == 0)
						_nitpicker_root_cap = root;
					else
						return false;

					if (_nitpicker_root_cap.valid())
						_nitpicker_root_lock.unlock();

					return true;
				}

				Genode::Service *resolve_session_request(const char *service_name,
				                                         const char *args) override
				{
					if (Genode::strcmp(service_name, "Nitpicker") == 0)
						return &_nitpicker_service;

					return Genode::Slave_policy::resolve_session_request(service_name, args);
				}

				Root_capability nitpicker_root() const
				{
					Lock::Guard guard(_nitpicker_root_lock);
					return _nitpicker_root_cap;
				}
		};

		Policy         _policy;
		size_t   const _quota = 2*1024*1024;
		Slave          _slave;
		Root_client    _nitpicker_root;

	public:

		/**
		 * Constructor
		 *
		 * \param ep   entrypoint used for nitpicker child thread
		 * \param ram  RAM session used to allocate the configuration
		 *             dataspace
		 */
		Nit_fader_slave(Rpc_entrypoint &ep, Ram_session &ram,
		                Genode::Service &nitpicker_service,
		                Genode::Dataspace_capability ldso_ds)
		:
			_policy(ep, ram, nitpicker_service),
			_slave(ep, _policy, _quota, env()->ram_session_cap(), ldso_ds),
			_nitpicker_root(_policy.nitpicker_root())
		{
			visible(false);
		}

		Capability<Nitpicker::Session> nitpicker_session(char const *label)
		{
			enum { ARGBUF_SIZE = 128 };
			char argbuf[ARGBUF_SIZE];
			argbuf[0] = 0;

			/*
			 * Declare ram-quota donation
			 */
			enum { SESSION_METADATA = 8*1024 };
			Arg_string::set_arg(argbuf, sizeof(argbuf), "ram_quota", SESSION_METADATA);

			/*
			 * Set session label
			 */
			Arg_string::set_arg_string(argbuf, sizeof(argbuf), "label", label);

			Session_capability session_cap = _nitpicker_root.session(argbuf, Affinity());

			return static_cap_cast<Nitpicker::Session>(session_cap);
		}

		void visible(bool visible)
		{
			_policy.visible(visible);
		}
};

#endif /* _NIT_FADER_SLAVE_H_ */
