/*
 * \brief  Test for the sandbox API
 * \author Norman Feske
 * \date   2020-01-09
 */

/*
 * Copyright (C) 2020 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <base/component.h>
#include <log_session/log_session.h>
#include <base/session_object.h>
#include <sandbox/sandbox.h>
#include <timer_session/connection.h>

namespace Test {

	using namespace Genode;

	struct Log_session_component;
	struct Main;
}


struct Test::Log_session_component : Session_object<Log_session>
{
	template <typename... ARGS>
	Log_session_component(ARGS &&... args) : Session_object(args...) { }

	void write(String const &msg) override
	{
		/* omit line break and zero termination supplied by 'msg' */
		if (msg.size() > 1)
			log("local LOG service: ", Cstring(msg.string(), msg.size() - 2));
	}
};


struct Test::Main : Sandbox::Local_service_base::Wakeup
{
	Env &_env;

	Heap _heap { _env.ram(), _env.rm() };

	struct State_handler : Sandbox::State_handler
	{
		void handle_sandbox_state() override { }

	} _state_handler { };

	Sandbox _sandbox { _env, _state_handler };

	using Log_service = Sandbox::Local_service<Log_session_component>;

	Log_service _log_service { _sandbox, *this };

	/*
	 * Periodically update the child version to stress the child destruction,
	 * the closing of the session to the local LOG service, and and the child
	 * re-creation.
	 */
	unsigned _dummy_version = 1;

	void _handle_timer(Duration)
	{
		_dummy_version++;

		_update_sandbox_config();
	}

	Timer::Connection _timer { _env };

	Timer::Periodic_timeout<Main> _timeout_handler {
		_timer, *this, &Main::_handle_timer, Microseconds(250*1000) };

	void _generate_sandbox_config(Generator &g) const
	{
		g.node("parent-provides", [&] () {

			auto service_node = [&] (char const *name) {
				g.node("service", [&] () {
					g.attribute("name", name); }); };

			service_node("ROM");
			service_node("CPU");
			service_node("PD");
			service_node("LOG");
		});

		g.node("start", [&] () {
			g.attribute("name", "dummy");
			g.attribute("caps", 100);
			g.attribute("version", _dummy_version);
			g.node("resource", [&] () {
				g.attribute("name", "RAM");
				g.attribute("quantum", "2M");
			});

			g.node("config", [&] () {
				g.node("log", [&] () {
					g.attribute("string", "started"); });
				g.node("create_log_connections", [&] () {
					g.attribute("ram_upgrade", "100K");
					g.attribute("count", "1"); });
				g.node("log", [&] () {
					g.attribute("string", "done"); });
			});

			g.node("route", [&] () {

				g.node("service", [&] () {
					g.attribute("name", "LOG");
					g.node("local", [&] () { }); });

				g.node("any-service", [&] () {
					g.node("parent", [&] () { }); });
			});
		});
	}

	void _update_sandbox_config()
	{
		Generated_node( _heap, 16*1024, "config", [&] (Generator &g) {
			_generate_sandbox_config(g); }
		).node.with_result(
			[&] (Node const &config) {
				log("generated config: ", config);
				_sandbox.apply_config(config);
			},
			[&] (Buffer_error) { error("failed to generate config"); }
		);
	}

	/**
	 * Sandbox::Local_service_base::Wakeup interface
	 */
	void wakeup_local_service() override
	{
		_log_service.for_each_requested_session([&] (Log_service::Request &request) {

			Log_session_component &session = *new (_heap)
				Log_session_component(_env.ep(), request.resources, request.label);

			request.deliver_session(session);
		});

		_log_service.for_each_upgraded_session([&] (Log_session_component &,
		                                            Session::Resources const &amount) {
			log("received RAM upgrade of ", amount.ram_quota);
			return Log_service::Upgrade_response::CONFIRMED;
		});

		_log_service.for_each_session_to_close([&] (Log_session_component &session) {
			destroy(_heap, &session);
			return Log_service::Close_response::CLOSED;
		});
	}

	Main(Env &env) : _env(env)
	{
		_update_sandbox_config();
	}
};


void Component::construct(Genode::Env &env) { static Test::Main main(env); }

