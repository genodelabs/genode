/*
 * \brief  Utility for implementing a local service with a single session
 * \author Norman Feske
 * \date   2014-02-14
 */

/*
 * Copyright (C) 2014 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _SINGLE_SESSION_SERVICE_H_
#define _SINGLE_SESSION_SERVICE_H_

#include <base/service.h>

namespace Wm { class Single_session_service; }

struct Wm::Single_session_service : Genode::Service
{
	Genode::Session_capability session_cap;

	Single_session_service(char const *service_name,
	                       Genode::Session_capability session_cap)
	:
		Service(service_name), session_cap(session_cap)
	{ }

	Genode::Session_capability
	session(const char *, Genode::Affinity const &) override
	{
		return session_cap;
	}

	void upgrade(Genode::Session_capability, const char *) override { }
	void close(Genode::Session_capability) override { }
};

#endif /* _SINGLE_SESSION_SERVICE_H_ */
