/*
 * \brief  Multiprocessor test for a server having multiple Rpc_entrypoints on
 *         different CPUs
 * \author Alexander Boettcher
 * \date   2013-07-19
 */

/*
 * Copyright (C) 2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <base/log.h>
#include <base/thread.h>
#include <base/env.h>
#include <base/sleep.h>

#include <cap_session/connection.h>
#include <base/rpc_server.h>

namespace Test {

	/**
	 * Test session interface definition
	 */
	struct Session : Genode::Session
	{
		static const char *service_name() { return "MP_RPC_TEST"; }

		GENODE_RPC(Rpc_test_untyped, void, test_untyped, unsigned);
		GENODE_RPC(Rpc_test_cap, void, test_cap, Genode::Native_capability);
		GENODE_RPC(Rpc_test_cap_reply, Genode::Native_capability,
		           test_cap_reply, Genode::Native_capability);
		GENODE_RPC_INTERFACE(Rpc_test_untyped, Rpc_test_cap, Rpc_test_cap_reply);
	};

	struct Client : Genode::Rpc_client<Session>
	{
		Client(Capability<Session> cap) : Rpc_client<Session>(cap) { }

		void test_untyped(unsigned value) { call<Rpc_test_untyped>(value); }
		void test_cap(Genode::Native_capability cap) { call<Rpc_test_cap>(cap); }
		Genode::Native_capability test_cap_reply(Genode::Native_capability cap) {
			return call<Rpc_test_cap_reply>(cap); }
	};

	struct Component : Genode::Rpc_object<Session, Component>
	{
		/* Test to just sent plain words (untyped items) */
		void test_untyped(unsigned);
		/* Test to transfer a object capability during send */
		void test_cap(Genode::Native_capability);
		/* Test to transfer a object capability during send+reply */
		Genode::Native_capability test_cap_reply(Genode::Native_capability);
	};

	typedef Genode::Capability<Session> Capability;

	/**
	 * Session implementation
	 */
	void Component::test_untyped(unsigned value) {
		Genode::log("function ", __FUNCTION__, ": got value ", value);
	}

	void Component::test_cap(Genode::Native_capability cap) {
		Genode::log("function ", __FUNCTION__, ": capability is valid ? ",
		            cap.valid() ? "yes" : "no", " - idx ", cap.local_name());
	}

	Genode::Native_capability Component::test_cap_reply(Genode::Native_capability cap) {
		Genode::log("function ", __FUNCTION__, ": capability is valid ? ",
		            cap.valid() ? "yes" : "no", " - idx ", cap.local_name());
		return cap;
	}
}

/**
 * Set up a server running on every CPU one Rpc_entrypoint
 */
int main(int argc, char **argv)
{
	using namespace Genode;

	log("--- test-mp_server started ---");

	Affinity::Space cpus = env()->cpu_session()->affinity_space();
	log("Detected ", cpus.width(), "x", cpus.height(), " CPU",
	       cpus.total() > 1 ? "s." : ".");

	enum { STACK_SIZE = 2*1024*sizeof(long) };

	static Cap_connection cap;
	Rpc_entrypoint ** eps = new (env()->heap()) Rpc_entrypoint*[cpus.total()];
	for (unsigned i = 0; i < cpus.total(); i++)
		eps[i] = new (env()->heap()) Rpc_entrypoint(&cap, STACK_SIZE, "rpc en",
		                                            true, cpus.location_of_index(i));

	/* XXX using the same object and putting it to different queues fails XXX */
	Test::Component * components = new (env()->heap()) Test::Component[cpus.total()];

	Test::Capability * caps = new (env()->heap()) Test::Capability[cpus.total()];
	for (unsigned i = 0; i < cpus.total(); i++)
		caps[i] = eps[i]->manage(&components[i]);

	Test::Client ** clients = new (env()->heap()) Test::Client*[cpus.total()];
	for (unsigned i = 0; i < cpus.total(); i++)
		clients[i] = new (env()->heap()) Test::Client(caps[i]);

	/* Test: Invoke RPC entrypoint on different CPUs */
	for (unsigned i = 0; i < cpus.total(); i++) {
		log("call server on CPU ", i);
		clients[i]->test_untyped(i);
	}

	/* Test: Transfer a capability to RPC Entrypoints on different CPUs */
	for (unsigned i = 0; i < cpus.total(); i++) {
		Native_capability cap = caps[0];
		log("call server on CPU ", i, " - transfer cap ", cap.local_name());
		clients[i]->test_cap(cap);
	}

	/* Test: Transfer a capability to RPC Entrypoints and back */
	for (unsigned i = 0; i < cpus.total(); i++) {
		Native_capability cap  = caps[0];
		log("call server on CPU ", i, " - transfer cap ", cap.local_name());
		Native_capability rcap = clients[i]->test_cap_reply(cap);
		log("got from server on CPU ", i, " - received cap ", rcap.local_name());
	}

	log("done");

	sleep_forever();
}
