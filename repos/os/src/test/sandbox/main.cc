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
#include <os/buffered_xml.h>
#include <os/sandbox.h>

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

	typedef Sandbox::Local_service<Log_session_component> Log_service;

	Log_service _log_service { _sandbox, *this };

	void _generate_sandbox_config(Xml_generator &xml) const
	{
		xml.node("parent-provides", [&] () {

			auto service_node = [&] (char const *name) {
				xml.node("service", [&] () {
					xml.attribute("name", name); }); };

			service_node("ROM");
			service_node("CPU");
			service_node("PD");
			service_node("LOG");
		});

		xml.node("start", [&] () {
			xml.attribute("name", "dummy");
			xml.attribute("caps", 100);
			xml.node("resource", [&] () {
				xml.attribute("name", "RAM");
				xml.attribute("quantum", "2M");
			});

			xml.node("config", [&] () {
				xml.node("log", [&] () {
					xml.attribute("string", "started"); });
				xml.node("create_log_connections", [&] () {
					xml.attribute("ram_upgrade", "100K");
					xml.attribute("count", "1"); });
				xml.node("destroy_log_connections", [&] () { });
				xml.node("log", [&] () {
					xml.attribute("string", "done"); });
				xml.node("exit", [&] () { });
			});

			xml.node("route", [&] () {

				xml.node("service", [&] () {
					xml.attribute("name", "LOG");
					xml.node("local", [&] () { }); });

				xml.node("any-service", [&] () {
					xml.node("parent", [&] () { }); });
			});
		});
	}

	/**
	 * Sandbox::Local_service_base::Wakeup interface
	 */
	void wakeup_local_service() override
	{
		_log_service.for_each_requested_session([&] (Log_service::Request &request) {

			Log_session_component &session = *new (_heap)
				Log_session_component(_env.ep(),
				                      request.resources,
				                      request.label,
				                      request.diag);

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
		Buffered_xml const config { _heap, "config", [&] (Xml_generator &xml) {
			_generate_sandbox_config(xml); } };

		config.with_xml_node([&] (Xml_node const &config) {

			log("generated config: ", config);

			_sandbox.apply_config(config);
		});
	}
};


void Component::construct(Genode::Env &env) { static Test::Main main(env); }

