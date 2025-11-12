/*
 * \brief  Sculpt deploy management
 * \author Norman Feske
 * \date   2018-04-30
 */

/*
 * Copyright (C) 2018 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* local includes */
#include <deploy.h>
#include <string.h>


bool Sculpt::Deploy::update_child_conditions()
{
	/* return true if any condition changed */
	return _children.apply_condition([&] (Node const &start, Node const &launcher) {

		/* the child cannot run as long as any dependency is missing */
		bool condition = true;
		_for_each_missing_server(start, [&] (Start_name const &) {
			condition = false; });
		_for_each_missing_server(launcher, [&] (Start_name const &) {
			condition = false; });

		return condition;
	});
}


void Sculpt::Deploy::view_diag(Scope<> &s) const
{
	/*
	 * Collect messages in registry, avoiding duplicates
	 */
	using Message = String<64>;
	using Registered_message = Registered_no_delete<Message>;
	Registry<Registered_message> messages { };

	auto gen_missing_dependencies = [&] (Node const &start, Start_name const &name)
	{
		_for_each_missing_server(start, [&] (Start_name const &server) {

			Message const new_message(Pretty(name), " requires ", Pretty(server));

			bool already_exists = false;
			messages.for_each([&] (Registered_message const &message) {
				if (message == new_message)
					already_exists = true; });

			if (!already_exists)
				new (_alloc) Registered_message(messages, new_message);
		});
	};

	_children.for_each_unsatisfied_child([&] (Node       const &start,
	                                          Node       const &launcher,
	                                          Start_name const &name) {
		gen_missing_dependencies(start,    name);
		gen_missing_dependencies(launcher, name);
	});

	/*
	 * Generate dialog elements, drop consumed messages from the registry
	 */
	messages.for_each([&] (Registered_message &message) {
		s.sub_scope<Left_annotation>(message);
		destroy(_alloc, &message);
	});
}


void Sculpt::Deploy::_handle_managed_deploy(Node const &managed_deploy)
{
	/* determine CPU architecture of deployment */
	Arch const orig_arch = _arch;
	_arch = managed_deploy.attribute_value("arch", Arch());
	if ((managed_deploy.type() != "empty") && !_arch.valid())
		warning("managed deploy config lacks 'arch' attribute");

	bool const arch_changed = (orig_arch != _arch);

	auto apply_config = [&]
	{
		try { return _children.apply_config(managed_deploy); }
		catch (...) {
			error("spurious exception during deploy update (apply_config)"); }
		return false;
	};

	bool const config_affected_child = apply_config();

	auto apply_launchers = [&]
	{
		bool any_child_affected = false;

		_launcher_listing_rom.with_node([&] (Node const &listing) {
			listing.for_each_sub_node("dir", [&] (Node const &dir) {

				using Path = String<20>;
				Path const path = dir.attribute_value("path", Path());

				if (path != "/launcher")
					return;

				dir.for_each_sub_node("file", [&] (Node const &file) {

					if (file.attribute_value("xml", false) == false)
						return;

					using Name = Depot_deploy::Child::Launcher_name;
					Name const name = file.attribute_value("name", Name());

					file.for_each_sub_node("launcher", [&] (Node const &launcher) {
						if (_children.apply_launcher(name, launcher))
							any_child_affected = true; });
				});
			});
		});
		return any_child_affected;
	};

	bool const launcher_affected_child = apply_launchers();

	auto apply_blueprint = [&]
	{
		bool progress = false;
		try {
			_blueprint_rom.with_node([&] (Node const &blueprint) {

			/* apply blueprint, except when stale */
				using Version = String<32>;
				Version const version = blueprint.attribute_value("version", Version());
				if (version == Version(_depot_query.depot_query_version().value))
					progress = _children.apply_blueprint(blueprint);
			});
		}
		catch (...) {
			error("spurious exception during deploy update (apply_blueprint)"); }
		return progress;
	};

	bool const blueprint_affected_child = apply_blueprint();

	bool const progress = arch_changed
	                   || config_affected_child
	                   || launcher_affected_child
	                   || blueprint_affected_child;
	if (progress) {

		/* update query for blueprints of all unconfigured start nodes */
		if (!_download_queue.any_active_download())
			_depot_query.trigger_depot_query();

		/* feed missing packages to installation queue */
		update_installation();

		/* apply runtime condition checks */
		update_child_conditions();

		_action.refresh_deploy_dialog();
		_runtime_config_generator.generate_runtime_config();
	}
}


void Sculpt::Deploy::gen_runtime_start_nodes(Generator      &g,
                                             Prio_levels     prio_levels,
                                             Affinity::Space affinity_space) const
{
	/* depot-ROM instance for regular (immutable) depot content */
	g.node("start", [&] {
		gen_fs_rom_start_content(g, "cached_fs_rom", "depot",
		                         cached_depot_rom_state); });

	/* depot-ROM instance for mutable content (/depot/local/) */
	g.node("start", [&] {
		gen_fs_rom_start_content(g, "fs_rom", "depot",
		                         uncached_depot_rom_state); });

	g.node("start", [&] {
		gen_depot_query_start_content(g); });

	_managed_deploy_rom.with_node([&] (Node const &managed_deploy) {

		/* insert content of '<static>' node as is */
		managed_deploy.with_optional_sub_node("static",
			[&] (Node const &static_config) {
				(void)g.append_node_content(static_config, { 20 }); });

		/* generate start nodes for deployed packages */
		managed_deploy.with_optional_sub_node("common_routes",
			[&] (Node const &common_routes) {
				_children.gen_start_nodes(g, common_routes,
				                          prio_levels, affinity_space,
				                          "depot_rom", "dynamic_depot_rom",
				                          [] (auto const &) { return true; });
				g.node("monitor", [&] {
					_children.gen_monitor_policy_nodes(g);});
			});
	});
}
