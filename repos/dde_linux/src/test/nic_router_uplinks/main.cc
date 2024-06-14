/*
 * \brief  Server component for Network Address Translation on NIC sessions
 * \author Martin Stein
 * \date   2016-08-24
 */

/*
 * Copyright (C) 2016-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode */
#include <base/component.h>
#include <base/attached_rom_dataspace.h>
#include <os/reporter.h>
#include <timer_session/connection.h>

/* local */
#include <assertion.h>

using namespace Genode;

enum { STEP_TIMEOUT_US = 60*1000*1000 };

using Driver = String<16>;

struct Result
{
	enum { INVALID_ID = ~0U };
	enum Type { REPLY, DESTINATION_UNREACHABLE, INVALID };
	using Type_string = String<32>;

	unsigned id;
	Type type;

	static Type string_to_type(Type_string const &string)
	{
		if (string == "reply") return REPLY;
		if (string == "destination_unreachable") return DESTINATION_UNREACHABLE;
		return INVALID;
	}

	Result(Xml_node const &node)
	:
		id(node.attribute_value("id", (unsigned)INVALID_ID)),
		type(string_to_type(node.attribute_value("type", Type_string())))
	{
		ASSERT(id != INVALID_ID && type != INVALID);
	}
};

struct Goal
{
	Result::Type result_type;
	unsigned count;
};

struct Main
{
	Env &env;
	Expanding_reporter router_config_reporter { env, "config", "router_config" };
	Attached_rom_dataspace result_rom { env, "ping_result" };
	Signal_handler<Main> result_handler { env.ep(), *this, &Main::handle_result };
	unsigned last_result_id = Result::INVALID_ID;
	Constructible<Goal> goal { };
	Timer::Connection timer { env };
	Timer::One_shot_timeout<Main> timeout { timer, *this, &Main::handle_timeout };
	unsigned step { 0UL };
	Constructible<Driver> driver { };

	void handle_result()
	{
		if (!goal.constructed()) {
			step_succeeded();
			return;
		}
		result_rom.update();
		Result result(result_rom.xml());
		if (result.id != last_result_id) {
			if (result.type == goal->result_type) {
				ASSERT(goal->count);
				if (!--goal->count) {
					goal.destruct();
					step_succeeded();
				}
			} else
				warning("test step ", step, " observed unexpected result");
			last_result_id = result.id;
		}
	}

	void handle_timeout(Duration)
	{
		error("test step ", step, " timed out");
		env.parent().exit(-1);
	}

	void update_router_config()
	{
		router_config_reporter.generate([&] (Xml_generator &xml) {
			xml.attribute("dhcp_discover_timeout_sec", "1");
			xml.node("policy", [&] {
				xml.attribute("label_prefix", "ping");
				xml.attribute("domain", "downlink"); });
			xml.node("policy", [&] {
				xml.attribute("label_prefix", "dhcp");
				xml.attribute("domain", "uplink"); });

			if (driver.constructed()) {
				xml.node("policy", [&] {
					xml.attribute("label_prefix", *driver);
					xml.attribute("domain", "uplink"); });
				xml.node("domain", [&] {
					xml.attribute("name", "uplink");
					xml.node("nat", [&] {
						xml.attribute("domain", "downlink");
						xml.attribute("icmp-ids", "999"); }); });
			}
			xml.node("domain", [&] {
				xml.attribute("name", "downlink");
				xml.attribute("interface", "10.0.1.79/24");
				xml.node("dhcp-server", [&] {
					xml.attribute("ip_first", "10.0.1.80");
					xml.attribute("ip_last", "10.0.1.100"); });

				if (driver.constructed())
					xml.node("icmp", [&] {
						xml.attribute("dst", "0.0.0.0/0");
						xml.attribute("domain", "uplink"); }); }); });
	}

	void start_step(unsigned step_arg, Driver driver_arg = Driver())
	{
		if (driver_arg == Driver()) {
			driver.destruct();
			goal.construct(Result::DESTINATION_UNREACHABLE, 3);
		} else {
			driver.construct(driver_arg);
			goal.construct(Result::REPLY, 3);
		}
		timeout.schedule(Microseconds(STEP_TIMEOUT_US));
		update_router_config();
		step = step_arg;
		log("test step ", step, " started");
	}

	void step_succeeded()
	{
		log("test step ", step, " succeeded");
		switch (step) {
		case 0: start_step(1, "nic"); break;
		case 1: start_step(2, "wifi"); break;
		case 2: start_step(3); break;
		case 3: start_step(4, "nic"); break;
		case 4: start_step(5, "nic"); break;
		case 5: start_step(6); break;
		case 6: start_step(7, "wifi"); break;
		case 7: start_step(8, "nic"); break;
		case 8: env.parent().exit(0); break;
		}
	}

	Main(Env &env) : env(env)
	{
		result_rom.sigh(result_handler);
		handle_result();
	}
};

void Component::construct(Env &env) { static Main main(env); }
