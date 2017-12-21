/*
 * \brief  RTC server
 * \author Christian Helmuth
 * \date   2015-01-06
 */

/*
 * Copyright (C) 2015-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode */
#include <base/component.h>
#include <base/heap.h>
#include <root/component.h>

#include "rtc.h"


namespace Rtc {
	using namespace Genode;

	struct Session_component;
	struct Root;
	struct Main;
}


struct Rtc::Session_component : public Genode::Rpc_object<Session>
{
	Env &_env;

	Timestamp current_time() override
	{
		Timestamp ret = Rtc::get_time(_env);

		return ret;
	}

	Session_component(Env &env) : _env(env) { }
};


class Rtc::Root : public Genode::Root_component<Session_component>
{
	private:

		Env &_env;

	protected:

		Session_component *_create_session(const char *)
		{
			return new (md_alloc()) Session_component(_env);
		}

	public:

		Root(Env &env, Allocator &md_alloc)
		:
			Genode::Root_component<Session_component>(&env.ep().rpc_ep(), &md_alloc),
			_env(env)
		{
			/* trigger initial RTC read */
			Rtc::get_time(_env);
		}
};


struct Rtc::Main
{
	Env &env;

	Sliced_heap sliced_heap { env.ram(), env.rm() };

	Root root { env, sliced_heap };

	Main(Env &env) : env(env) { env.parent().announce(env.ep().manage(root)); }
};


void Component::construct(Genode::Env &env) { static Rtc::Main main(env); }
