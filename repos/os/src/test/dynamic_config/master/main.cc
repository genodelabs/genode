/*
 * \brief  Test for changing the configuration of a slave at runtime
 * \author Norman Feske
 * \date   2012-04-04
 */

/*
 * Copyright (C) 2012-2016 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <base/component.h>
#include <os/slave.h>
#include <timer_session/connection.h>


namespace Test {

	using namespace Genode;

	struct Policy;
	struct Main;
}


struct Test::Policy : Genode::Slave::Policy
{
	const char **_permitted_services() const
	{
		static const char *permitted_services[] = {
			"CPU", "RAM", "ROM", "PD", "LOG", 0 };

		return permitted_services;
	};

	Policy(Genode::Env &env, Name const &name)
	:
		Genode::Slave::Policy(name, name, env.ep().rpc_ep(), env.rm(),
		                      env.ram_session_cap(), 1024*1024)
	{ }
};


struct Test::Main
{
	Env &_env;

	Policy _policy { _env, "test-dynamic_config" };

	unsigned _cnt = 0;

	void _configure()
	{
		String<256> const config("<config><counter>", _cnt, "</counter></config>");
		_policy.configure(config.string());
		_cnt++;
	}

	Child _child { _env.rm(), _env.ep().rpc_ep(), _policy };

	Timer::Connection timer { _env };

	Signal_handler<Main> _timeout_handler { _env.ep(), *this, &Main::_handle_timeout };

	void _handle_timeout() { _configure(); }

	Main(Env &env) : _env(env)
	{
		/* update slave config at regular intervals */
		timer.sigh(_timeout_handler);
		timer.trigger_periodic(250*1000);

		/* define initial config for slave before returning to entrypoint */
		_configure();
	}
};


void Component::construct(Genode::Env &env) { static Test::Main main(env); }
