/*
 * \brief  RTC server
 * \author Christian Helmuth
 * \date   2015-01-06
 */

/*
 * Copyright (C) 2015 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode */
#include <base/env.h>
#include <base/heap.h>
#include <os/server.h>
#include <root/component.h>
#include <rtc_session/rtc_session.h>

#include "rtc.h"


namespace Rtc {
	using namespace Genode;

	struct Session_component;
	struct Root;
	struct Main;
}


struct Rtc::Session_component : public Genode::Rpc_object<Session>
{
	Timestamp current_time() override
	{
		Timestamp ret = Rtc::get_time();

		return ret;
	}
};


class Rtc::Root : public Genode::Root_component<Session_component>
{
	protected:

		Session_component *_create_session(const char *args)
		{
			return new (md_alloc()) Session_component();
		}

	public:

		Root(Server::Entrypoint &ep, Allocator &md_alloc)
		:
			Genode::Root_component<Session_component>(&ep.rpc_ep(), &md_alloc)
		{
			/* trigger initial RTC read */
			Rtc::get_time();
		}
};


struct Rtc::Main
{
	Server::Entrypoint &ep;

	Sliced_heap sliced_heap { env()->ram_session(), env()->rm_session() };

	Root root { ep, sliced_heap };

	Main(Server::Entrypoint &ep) : ep(ep)
	{
		env()->parent()->announce(ep.manage(root));
	}
};


/**********************
 ** Server framework **
 **********************/

char const *   Server::name()                            { return "rtc_ep"; }
Genode::size_t Server::stack_size()                      { return 1024 * sizeof(long); }
void           Server::construct(Server::Entrypoint &ep) { static Rtc::Main inst(ep); }
