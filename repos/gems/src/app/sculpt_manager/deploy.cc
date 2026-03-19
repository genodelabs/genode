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


Genode::Progress Sculpt::Deploy::update_child_conditions()
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


void Sculpt::Deploy::_process_deploy(Node const &managed_deploy)
{
	/* determine CPU architecture of deployment */
	_arch = managed_deploy.attribute_value("arch", Arch());
	if ((managed_deploy.type() != "empty") && !_arch.valid())
		warning("managed deploy config lacks 'arch' attribute");

	auto apply_config = [&]
	{
		try { return _children.apply_deploy(managed_deploy); }
		catch (...) {
			error("spurious exception during deploy update (apply_config)"); }
		return STALLED;
	};

	Progress progress = enabled_options.update_from_deploy(managed_deploy);

	if (apply_config().progressed)
		progress = PROGRESSED;

	_launcher_listing_rom.with_node([&] (Node const &listing) {
		listing.for_each_sub_node("dir", [&] (Node const &dir) {

			using Path = String<20>;
			Path const path = dir.attribute_value("path", Path());

			if (path == "/launcher") {

				dir.for_each_sub_node("file", [&] (Node const &file) {

					if (file.attribute_value("xml", false) == false)
						return;

					using Name = Depot_deploy::Child::Launcher_name;
					Name const name = file.attribute_value("name", Name());

					file.for_each_sub_node("launcher", [&] (Node const &launcher) {
						if (_children.apply_launcher(name, launcher).progressed)
							progress = PROGRESSED; });
				});
			}

			if (path == "/option") {

				dir.for_each_sub_node("file", [&] (Node const &file) {

					using Name = Options::Name;
					Name const name = file.attribute_value("name", Name());

					file.for_each_sub_node("option", [&] (Node const &option) {
						if (_children.apply_option(name, option).progressed)
							progress = PROGRESSED; });
				});
			}
		});
	});

	try {
		_blueprint_rom.with_node([&] (Node const &blueprint) {

		/* apply blueprint, except when stale */
			using Version = String<32>;
			Version const version = blueprint.attribute_value("version", Version());
			if (version == Version(_blueprint_query.blueprint_query_version().value))
				if (_children.apply_blueprint(blueprint).progressed)
					progress = PROGRESSED;
		});
	}
	catch (...) {
		error("spurious exception during deploy update (apply_blueprint)"); }

	if (progress.stalled())
		return;

	/* apply runtime condition checks */
	if (update_child_conditions().progressed)
		_blueprint_query.query_blueprint();

	_action.refresh_deploy_dialog();
	_runtime_config_generator.generate_runtime_config();
}


void Sculpt::Deploy::gen_child_nodes(Generator &g) const
{
	/* depot-ROM instance for regular (immutable) depot content */
	g.node("child", [&] {
		gen_fs_rom_child_content(g, "depot", cached_depot_rom_state); });

	/* depot-ROM instance for mutable content (/depot/local/) */
	g.node("child", [&] {
		gen_fs_rom_child_content(g, "depot", uncached_depot_rom_state); });

	g.node("child", [&] {
		gen_blueprint_query_child_content(g); });
}


void Sculpt::Deploy::gen_runtime_start_nodes(Generator      &g,
                                             Node     const &deploy,
                                             Prio_levels     prio_levels,
                                             Affinity::Space affinity_space) const
{
	/* insert content of '<static>' node as is */
	deploy.with_optional_sub_node("static",
		[&] (Node const &static_config) {
			(void)g.append_node_content(static_config, { 20 }); });

	/* generate start nodes for deployed packages */
	deploy.with_optional_sub_node("common_routes",
		[&] (Node const &common_routes) {
			_children.gen_start_nodes(g, common_routes,
			                          prio_levels, affinity_space, "depot_rom",
			                          [] (auto const &) { return true; });
			g.node("monitor", [&] {
				_children.gen_monitor_policy_nodes(g);});
		});
}
