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

	using Name        = String<128>;
	using Prio_levels = Child::Prio_levels;
	using Arch        = String<16>;

	Prio_levels _prio_levels { };
	Arch        _arch { };

	void _handle_config()
	{
		_config.update();
		_blueprint.update();

		/*
		 * Capture original state, used to detect if the config has any effect
		 */
		struct Attributes
		{
			bool        state_reporter_constructed;
			Prio_levels prio_levels;
			Arch        arch;

			bool operator != (Attributes const &other) const
			{
				return other.state_reporter_constructed != state_reporter_constructed
				    || other.prio_levels.value          != prio_levels.value
				    || other.arch                       != arch;
			}
		};

		auto curr_attributes = [&] {
			return Attributes {
				.state_reporter_constructed = _state_reporter.constructed(),
				.prio_levels                = _prio_levels,
				.arch                       = _arch }; };

		Attributes const orig_attributes = curr_attributes();

		Xml_node const config = _config.xml();

		bool const report_state =
			config.has_sub_node("report") &&
			config.sub_node("report").attribute_value("state", false);

		_state_reporter.conditional(report_state, _env, "state", "state");

		_prio_levels = Child::Prio_levels { config.attribute_value("prio_levels", 0U) };
		_arch        = config.attribute_value("arch", Arch());

		bool const config_affected_child    = _children.apply_config(config);
		bool const blueprint_affected_child = _children.apply_blueprint(_blueprint.xml());

		bool const progress = (curr_attributes() != orig_attributes)
		                   || config_affected_child
		                   || blueprint_affected_child;
		if (!progress)
			return;

		if (_state_reporter.constructed())
			_state_reporter->generate([&] (Xml_generator xml) {
				xml.attribute("running", true); });

		if (!_arch.valid())
			warning("config lacks 'arch' attribute");

		/* generate init config containing all configured start nodes */
		_init_config_reporter.generate([&] (Xml_generator &xml) {
			_gen_init_config(xml, config); });

		/* update query for blueprints of all unconfigured start nodes */
		if (_arch.valid()) {
			_query_reporter.generate([&] (Xml_generator &xml) {
				xml.attribute("arch", _arch);
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

	void _gen_init_config(Xml_generator &xml, Xml_node const &config) const
	{
		if (_prio_levels.value)
			xml.attribute("prio_levels", _prio_levels.value);

		config.with_sub_node("static",
			[&] (Xml_node static_config) {
				static_config.with_raw_content([&] (char const *start, size_t length) {
					xml.append(start, length); });
			},
			[&] { warning("config lacks <static> node"); });

		config.with_optional_sub_node("report", [&] (Xml_node const &report) {

			auto copy_bool_attribute = [&] (auto const name)
			{
				if (report.has_attribute(name))
					xml.attribute(name, report.attribute_value(name, false));
			};

			auto copy_buffer_size_attribute = [&]
			{
				auto const name = "buffer";
				if (report.has_attribute(name))
					xml.attribute(name, report.attribute_value(name, Number_of_bytes(4096)));
			};

			size_t const delay_ms = report.attribute_value("delay_ms", 1000UL);
			xml.node("report", [&] {
				xml.attribute("delay_ms", delay_ms);

				/* attributes according to repos/os/src/lib/sandbox/report.h */
				copy_bool_attribute("ids");
				copy_bool_attribute("requested");
				copy_bool_attribute("provided");
				copy_bool_attribute("session_args");
				copy_bool_attribute("child_ram");
				copy_bool_attribute("child_caps");
				copy_bool_attribute("init_ram");
				copy_bool_attribute("init_caps");

				/* attribute according to repos/os/src/init/main.cc */
				copy_buffer_size_attribute();
			});
		});

		config.with_optional_sub_node("heartbeat", [&] (Xml_node const &heartbeat) {
			size_t const rate_ms = heartbeat.attribute_value("rate_ms", 2000UL);
			xml.node("heartbeat", [&] {
				xml.attribute("rate_ms", rate_ms);
			});
		});

		config.with_sub_node("common_routes",
			[&] (Xml_node node) {
				Child::Depot_rom_server const parent { };
				_children.gen_start_nodes(xml, node,
				                          _prio_levels, Affinity::Space(1, 1),
				                          parent, parent);
			},
			[&] { warning("config lacks <common_routes> node"); });
	}

	Main(Env &env) : _env(env)
	{
		_config   .sigh(_config_handler);
		_blueprint.sigh(_config_handler);

		_handle_config();
	}
};


void Component::construct(Genode::Env &env) { static Depot_deploy::Main main(env); }

