/*
 * \brief  Entrypoint test
 * \author Christian Helmuth
 * \date   2020-03-20
 */

/*
 * Copyright (C) 2020 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <base/component.h>
#include <base/rpc_server.h>
#include <base/rpc_client.h>
#include <timer_session/connection.h>

using namespace Genode;


/**
 * Test destruction of entrypoint with yet not dissolved components
 */
namespace Test_destruct {
	struct Session;
	struct Component;
}

struct Test_destruct::Session : Genode::Session
{
	static const char *service_name() { return "Test_destruct"; }

	GENODE_RPC(Rpc_test_untyped, void, test_untyped, unsigned);
	GENODE_RPC_INTERFACE(Rpc_test_untyped);
};

struct Test_destruct::Component : Genode::Rpc_object<Test_destruct::Session,
	                                                 Test_destruct::Component>
{
	void test_untyped(unsigned) { }
};


/**
 * Test signal handling with proxy and wait_and_dispatch_one_io_signal()
 */
namespace Test_signal {
	struct Session;
	struct Session_component;
	struct Entrypoint;

	enum { TIMER_DURATION = 10'000ul };
}


struct Test_signal::Session : Genode::Session
{
	static const char *service_name() { return "Test_signal"; }

	GENODE_RPC(Rpc_rpc, void, rpc);
	GENODE_RPC_INTERFACE(Rpc_rpc);
};


struct Test_signal::Session_component : Rpc_object<Test_signal::Session,
                                                   Test_signal::Session_component>
{
	Genode::Entrypoint &ep;

	Session_component(Genode::Entrypoint &ep) : ep(ep) { }

	void stats()
	{
		log("rpcs=", rpc_count, " signals=", sig_count, " timeout-signals=", sig_timeout_count);
	}

	unsigned rpc_count         { 0 };
	unsigned sig_count         { 0 };
	unsigned sig_timeout_count { 0 };

	void rpc()
	{
		++rpc_count;
		ep.wait_and_dispatch_one_io_signal();
	}

	void sig()         { ++sig_count;         }
	void sig_timeout() { ++sig_timeout_count; }
};


struct Test_signal::Entrypoint : Genode::Entrypoint
{
	Env &env;

	Session_component sc { *this };

	Capability<Session> cap { manage(sc) };

	Io_signal_handler<Test_signal::Entrypoint> sigh {
		*this, *this, &Test_signal::Entrypoint::handle_signal };

	Timer::Connection timer { env };

	Io_signal_handler<Test_signal::Entrypoint> timer_sigh {
		*this, *this, &Test_signal::Entrypoint::handle_timer_signal };

	Entrypoint(Env &env)
	:
		Genode::Entrypoint(env, 0x4000, "test_ep", Affinity::Location()),
		env(env)
	{
		timer.sigh(timer_sigh);
		timer.trigger_periodic(Test_signal::TIMER_DURATION/2);
	}

	void handle_signal()       { sc.sig(); }
	void handle_timer_signal() { sc.sig_timeout(); }
};


struct Main
{
	Env &env;

	Test_signal::Entrypoint test_ep { env };

	Timer::Connection timer { env };

	Signal_handler<Main> sigh { env.ep(), *this, &Main::handle_signal };

	unsigned rpc_count { 0 };

	Main(Env &env) : env(env)
	{
		timer.sigh(sigh);
		timer.trigger_periodic(Test_signal::TIMER_DURATION);
	}

	void handle_signal()
	{
		Signal_transmitter(test_ep.sigh).submit();
		test_ep.cap.call<Test_signal::Session::Rpc_rpc>();

		if (++rpc_count % 100 == 0)
			test_ep.sc.stats();
		if (rpc_count == 3'000)
			env.parent().exit(0);
	}
};


void Component::construct(Env &env)
{
	/* test destruct */
	Test_destruct::Component c;

	{
		Entrypoint ep(env, 0x4000, "test_destruct_ep", Affinity::Location());
		ep.manage(c);
	}

	/* test signal */
	static Main inst { env };
}
