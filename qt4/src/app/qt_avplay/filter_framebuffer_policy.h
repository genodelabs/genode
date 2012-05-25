/*
 * \brief   Filter framebuffer policy
 * \author  Christian Prochaska
 * \date    2012-04-11
 */

/*
 * Copyright (C) 2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _FILTER_FRAMEBUFFER_POLICY_H_
#define _FILTER_FRAMEBUFFER_POLICY_H_

/* Genode includes */
#include <base/service.h>
#include <os/slave.h>


class Filter_framebuffer_policy : public Genode::Slave_policy
{
	private:

		Genode::Service_registry &_framebuffer_in;
		Genode::Service_registry &_framebuffer_out;

	protected:

		const char **_permitted_services() const
		{
			static const char *permitted_services[] = {
				"CAP", "LOG", "RM", "ROM", "SIGNAL",
				"Timer", 0 };

			return permitted_services;
		};

	public:

		Filter_framebuffer_policy(const char *name,
		                          Genode::Rpc_entrypoint &entrypoint,
		                          Genode::Service_registry &framebuffer_in,
		                          Genode::Service_registry &framebuffer_out)
		: Genode::Slave_policy(name, entrypoint, Genode::env()->ram_session()),
		  _framebuffer_in(framebuffer_in),
		  _framebuffer_out(framebuffer_out) { }

		Genode::Service *resolve_session_request(const char *service_name,
		                                         const char *args)
		{
			if (strcmp(service_name, "Framebuffer") == 0) {
				Genode::Client client;
				return _framebuffer_in.wait_for_service(service_name, &client, name());
			}

			return Slave_policy::resolve_session_request(service_name, args);
		}

		bool announce_service(const char *name,
		                      Genode::Root_capability root,
		                      Genode::Allocator *alloc,
		                      Genode::Server *server)
		{
			if (strcmp(name, "Framebuffer") == 0) {
				_framebuffer_out.insert(new (alloc) Genode::Child_service(name, root, server));
				return true;
			}

			return Slave_policy::announce_service(name, root, alloc, server);
		}


};

#endif /* _FILTER_FRAMEBUFFER_POLICY_H_ */
