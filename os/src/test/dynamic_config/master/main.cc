/*
 * \brief  Test for changing the configuration of a slave at runtime
 * \author Norman Feske
 * \date   2012-04-04
 */

/*
 * Copyright (C) 2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <os/config.h>
#include <os/slave.h>
#include <timer_session/connection.h>
#include <cap_session/connection.h>


struct Test_slave_policy : Genode::Slave_policy
{
	const char **_permitted_services() const
	{
		static const char *permitted_services[] = {
			"RM", "LOG", "SIGNAL", 0 };

		return permitted_services;
	};

	Test_slave_policy(char const *name, Genode::Rpc_entrypoint &ep)
	: Genode::Slave_policy(name, ep, Genode::env()->ram_session())
	{ }
};


int main(int, char **)
{
	using namespace Genode;

	enum { STACK_SIZE = 2*4096 };
	static Cap_connection cap;
	static Rpc_entrypoint ep(&cap, STACK_SIZE, "slave_ep");

	static Test_slave_policy slave_policy("test-dynamic_config", ep);

	/* define initial config for slave */
	slave_policy.configure("<config><counter>-1</counter></config>");

	static Genode::Slave slave(ep, slave_policy, 768*1024);

	/* update slave config at regular intervals */
	int counter = 0;
	for (;;) {

		static Timer::Connection timer;
		timer.msleep(250);

		/* re-generate configuration */
		char buf[100];
		Genode::snprintf(buf, sizeof(buf),
		                 "<config><counter>%d</counter></config>",
		                 counter++);

		slave_policy.configure(buf);
	}
	return 0;
}
