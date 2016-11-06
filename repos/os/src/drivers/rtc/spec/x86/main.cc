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
#include <base/component.h>
#include <base/heap.h>
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

		Root(Entrypoint &ep, Allocator &md_alloc)
		:
			Genode::Root_component<Session_component>(&ep.rpc_ep(), &md_alloc)
		{
			/* trigger initial RTC read */
			Rtc::get_time();
		}
};


struct Rtc::Main
{
	Env &env;

	Sliced_heap sliced_heap { env.ram(), env.rm() };

	Root root { env.ep(), sliced_heap };

	Main(Env &env) : env(env) { env.parent().announce(env.ep().manage(root)); }
};


void Component::construct(Genode::Env &env) { static Rtc::Main main(env); }
