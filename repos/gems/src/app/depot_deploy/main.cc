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
#include <util/reconstructible.h>
#include <base/component.h>
#include <base/attached_rom_dataspace.h>
#include <base/heap.h>
#include <os/reporter.h>

/* local includes */
#include "children.h"

namespace Depot_deploy { struct Main; }


struct Depot_deploy::Main : Option::Action
{
	Env &_env;

	Attached_rom_dataspace _config    { _env, "config" },
	                       _deploy    { _env, "deploy" },
	                       _blueprint { _env, "blueprint" };

	Constructible<Attached_rom_dataspace> _launchers { };

	Signal_handler<Main>
		_config_handler    { _env.ep(), *this, &Main::_handle_config },
		_deploy_handler    { _env.ep(), *this, &Main::_handle_deploy },
		_blueprint_handler { _env.ep(), *this, &Main::_handle_blueprint },
		_launchers_handler { _env.ep(), *this, &Main::_handle_launchers };

	struct Attr
	{
		using Arch = String<16>;

		bool            verbose;
		Arch            arch;
		Prio_levels     prio_levels;
		Affinity::Space affinity_space;
		bool            state_reporter;
		Num_bytes       blueprint_buffer;
		bool            launchers;

		/*
		 * Child providing depot content as ROM modules.
		 * If undefined, the ROMs are requested from the parent.
		 */
		Depot_rom_server depot_rom;

		static Attr from_config(Node const &config)
		{
			return {
				.verbose     =   config.attribute_value("verbose", false),
				.arch        =   config.attribute_value("arch", Arch()),
				.prio_levels = { config.attribute_value("prio_levels", 0U) },

				.affinity_space = config.with_sub_node("affinity-space",
					[&] (Node const &node) { return Affinity::Space::from_node(node); },
					[&]                    { return Affinity::Space(1, 1); }),

				.state_reporter = config.with_sub_node("report",
					[] (Node const &node) { return node.attribute_value("state", false); },
					[]                    { return false; }),

				.blueprint_buffer = config.attribute_value("blueprint_buffer", Num_bytes { }),

				.launchers = config.attribute_value("launchers", false),
				.depot_rom = config.attribute_value("depot_rom", Depot_rom_server { }),
			};
		}
	};

	Attr _attr { };

	Expanding_reporter _query_reporter   { _env, "query" , "query"};
	Expanding_reporter _runtime_reporter { _env, "config", "runtime"};

	Constructible<Expanding_reporter> _state_reporter { };

	Heap _heap { _env.ram(), _env.rm() };

	Children _children { _heap };

	void _update_runtime_and_query()
	{
		if (_state_reporter.constructed())
			_state_reporter->generate([&] (Generator &g) {
				g.attribute("running", true); });

		/* generate init config containing all configured start nodes */
		_runtime_reporter.generate([&] (Generator &g) {
			_gen_runtime(g, _config.node()); });

		/* update query for blueprints of all unconfigured start nodes */
		if (_attr.arch.valid()) {
			_query_reporter.generate([&] (Generator &g) {
				g.attribute("arch", _attr.arch);
				if (_attr.blueprint_buffer > 0u)
					g.attribute("blueprint_buffer", _attr.blueprint_buffer);
				_children.gen_queries(g);
			});
		}

		if ((_children.count() > 0UL)
		 && !_children.any_incomplete()
		 && !_children.any_blueprint_needed()
		 && _state_reporter.constructed()) {

			_state_reporter->generate([&] (Generator &g) {
				g.attribute("running", false);
				g.attribute("count", _children.count());
			});
		}
	}

	void _handle_deploy()
	{
		_deploy.update();

		if (_children.apply_deploy(_deploy.node()).progressed) {

			_apply_launchers_to_children();

			/* a new option may have appeared in the deploy config */
			_children.watch_options(_env, *this);

			_update_runtime_and_query();
		}
	}

	void _handle_blueprint()
	{
		_blueprint.update();

		if (_children.apply_blueprint(_blueprint.node()).progressed)
			_update_runtime_and_query();
	}

	Progress _apply_launchers_to_children()
	{
		if (!_launchers.constructed())
			return STALLED;

		Progress progress = STALLED;
		_launchers->node().with_optional_sub_node("dir", [&] (Node const &dir) {
			dir.for_each_sub_node("file", [&] (Node const &file) {
				using Name = Child::Launcher_name;
				Name const name = file.attribute_value("name", Name());
				file.with_optional_sub_node("launcher", [&] (Node const &launcher) {
					if (_children.apply_launcher(name, launcher).progressed)
						progress = PROGRESSED; }); }); });
		return progress;
	}

	void _handle_launchers()
	{
		if (!_launchers.constructed())
			return;

		_launchers->update();

		if (_apply_launchers_to_children().progressed)
			_update_runtime_and_query();
	}

	void _handle_config()
	{
		_config.update();

		_attr = Attr::from_config(_config.node());

		if (!_attr.arch.valid())
			warning("config lacks 'arch' attribute");

		_state_reporter.conditional(_attr.state_reporter, _env, "state", "state");

		if (_attr.launchers && !_launchers.constructed()) {
			_launchers.construct(_env, "launchers");
			_launchers->sigh(_launchers_handler);
			_launchers_handler.local_submit();
		}

		_update_runtime_and_query();
	}

	/**
	 * Option::Action interface
	 */
	void deploy_option_changed(Option::Name const &name, Node const &config) override
	{
		if (_children.apply_option(name, config).progressed)
			_handle_config();
	}

	void _gen_runtime(Generator &g, Node const &config) const
	{
		if (_attr.verbose)
			g.attribute("verbose", "yes");

		if (_attr.prio_levels.value)
			g.attribute("prio_levels", _attr.prio_levels.value);

		config.with_sub_node("static",
			[&] (Node const &static_config) {
				if (!g.append_node_content(static_config, MAX_NODE_DEPTH))
					warning("config too deeply nested: ", static_config); },
			[&] { warning("config lacks <static> node"); });

		auto copy_nodes = [&] (auto const &node_type)
		{
			config.for_each_sub_node(node_type, [&] (Node const &node) {
				(void)g.append_node(node, Generator::Max_depth { 2 }); });
		};

		copy_nodes("affinity-space");
		copy_nodes("report");
		copy_nodes("heartbeat");
		copy_nodes("alias");

		config.with_sub_node("common_routes",
			[&] (Node const &node) {
				_children.gen_start_nodes(g, node,
				                          _attr.prio_levels, _attr.affinity_space,
				                          _attr.depot_rom,
				                          [] (Child::Name const &) { return true; });
			},
			[&] { warning("config lacks <common_routes> node"); });
	}

	Main(Env &env) : _env(env)
	{
		_config   .sigh(_config_handler);    _handle_config();
		_deploy   .sigh(_deploy_handler);    _handle_deploy();
		_blueprint.sigh(_blueprint_handler); _handle_blueprint();
	}
};


void Component::construct(Genode::Env &env) { static Depot_deploy::Main main(env); }

