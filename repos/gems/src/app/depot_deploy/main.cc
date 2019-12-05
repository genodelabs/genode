/*
 * \brief  Tool for turning a subsystem blueprint into an init configuration
 * \author Norman Feske
 * \date   2017-07-07
 */

/*
 * Copyright (C) 2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <util/retry.h>
#include <util/reconstructible.h>
#include <base/component.h>
#include <base/attached_rom_dataspace.h>
#include <base/heap.h>
#include <os/reporter.h>

/* local includes */
#include "children.h"

namespace Depot_deploy { struct Main; }


struct Depot_deploy::Main
{
	Env &_env;

	Attached_rom_dataspace _config    { _env, "config" };
	Attached_rom_dataspace _blueprint { _env, "blueprint" };

	Expanding_reporter _query_reporter       { _env, "query" , "query"};
	Expanding_reporter _init_config_reporter { _env, "config", "init.config"};

	Constructible<Expanding_reporter> _state_reporter { };

	Heap _heap { _env.ram(), _env.rm() };

	Children _children { _heap };

	Signal_handler<Main> _config_handler {
		_env.ep(), *this, &Main::_handle_config };

	typedef String<128> Name;

	void _handle_config()
	{
		_config.update();
		_blueprint.update();

		bool const report_state =
			_config.xml().has_sub_node("report") &&
			_config.xml().sub_node("report").attribute_value("state", false);

		_state_reporter.conditional(report_state, _env, "state", "state");

		if (_state_reporter.constructed())
			_state_reporter->generate([&] (Xml_generator xml) {
				xml.attribute("running", true); });

		Xml_node const config = _config.xml();

		_children.apply_config(config);
		_children.apply_blueprint(_blueprint.xml());

		/* determine CPU architecture of deployment */
		typedef String<16> Arch;
		Arch const arch = config.attribute_value("arch", Arch());
		if (!arch.valid())
			warning("config lacks 'arch' attribute");

		/* generate init config containing all configured start nodes */
		_init_config_reporter.generate([&] (Xml_generator &xml) {

			Xml_node static_config = config.sub_node("static");
			static_config.with_raw_content([&] (char const *start, size_t length) {
				xml.append(start, length); });

			config.with_sub_node("report", [&] (Xml_node const &report) {
				size_t const delay_ms = report.attribute_value("delay_ms", 1000UL);
				xml.node("report", [&] () {
					xml.attribute("delay_ms", delay_ms);
				});
			});

			config.with_sub_node("heartbeat", [&] (Xml_node const &heartbeat) {
				size_t const rate_ms = heartbeat.attribute_value("rate_ms", 2000UL);
				xml.node("heartbeat", [&] () {
					xml.attribute("rate_ms", rate_ms);
				});
			});

			Child::Depot_rom_server const parent { };
			_children.gen_start_nodes(xml, config.sub_node("common_routes"),
			                          parent, parent);
		});

		/* update query for blueprints of all unconfigured start nodes */
		if (arch.valid()) {
			_query_reporter.generate([&] (Xml_generator &xml) {
				xml.attribute("arch", arch);
				_children.gen_queries(xml);
			});
		}

		if ((_children.count() > 0UL)
		 && !_children.any_incomplete()
		 && !_children.any_blueprint_needed()
		 && _state_reporter.constructed()) {

			_state_reporter->generate([&] (Xml_generator xml) {
				xml.attribute("running", false);
				xml.attribute("count", _children.count());
			});
		}
	}

	Main(Env &env) : _env(env)
	{
		_config   .sigh(_config_handler);
		_blueprint.sigh(_config_handler);


		_handle_config();
	}
};


void Component::construct(Genode::Env &env) { static Depot_deploy::Main main(env); }

