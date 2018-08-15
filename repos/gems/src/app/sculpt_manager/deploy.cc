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


bool Sculpt::Deploy::update_child_conditions()
{
	/* return true if any condition changed */
	return _children.apply_condition([&] (Xml_node start, Xml_node launcher) {

		/* the child cannot run as long as any dependency is missing */
		bool condition = true;
		_for_each_missing_server(start, [&] (Start_name const &) {
			condition = false; });
		_for_each_missing_server(launcher, [&] (Start_name const &) {
			condition = false; });

		return condition;
	});
}


void Sculpt::Deploy::_gen_missing_dependencies(Xml_generator &xml, Start_name const &name,
                                               Xml_node start, int &count) const
{
	_for_each_missing_server(start, [&] (Start_name const &server) {
		gen_named_node(xml, "hbox", String<20>(count++), [&] () {
			gen_named_node(xml, "float", "left", [&] () {
				xml.attribute("west", "yes");
				xml.node("label", [&] () {
					xml.attribute("text", String<64>(name, " requires ", server));
					xml.attribute("font", "annotation/regular");
				});
			});
		});
	});
}


void Sculpt::Deploy::gen_child_diagnostics(Xml_generator &xml) const
{
	bool all_children_ok = true;
	_children.for_each_unsatisfied_child([&] (Xml_node, Xml_node) {
		all_children_ok = false; });

	if (all_children_ok)
		return;

	int count = 0;
	gen_named_node(xml, "frame", "diagnostics", [&] () {
		xml.node("vbox", [&] () {

			xml.node("label", [&] () {
				xml.attribute("text", "Diagnostics"); });

			xml.node("float", [&] () {
				xml.node("vbox", [&] () {
					_children.for_each_unsatisfied_child([&] (Xml_node start, Xml_node launcher) {
						Start_name const name = start.attribute_value("name", Start_name());
						_gen_missing_dependencies(xml, name, start,    count);
						_gen_missing_dependencies(xml, name, launcher, count);
					});
				});
			});
		});
	});
}


void Sculpt::Deploy::handle_deploy()
{
	Xml_node const manual_deploy = _manual_deploy_rom.xml();

	/* determine CPU architecture of deployment */
	_arch = manual_deploy.attribute_value("arch", Arch());
	if (!_arch.valid())
		warning("manual deploy config lacks 'arch' attribute");

	try { _children.apply_config(manual_deploy); }
	catch (...) {
		error("spurious exception during deploy update (apply_config)"); }

	/*
	 * Apply launchers
	 */
	Xml_node const launcher_listing = _launcher_listing_rom.xml();
	launcher_listing.for_each_sub_node("dir", [&] (Xml_node dir) {

		typedef String<20> Path;
		Path const path = dir.attribute_value("path", Path());

		if (path != "/launcher")
			return;

		dir.for_each_sub_node("file", [&] (Xml_node file) {

			if (file.attribute_value("xml", false) == false)
				return;

			typedef Depot_deploy::Child::Launcher_name Name;
			Name const name = file.attribute_value("name", Name());

			file.for_each_sub_node("launcher", [&] (Xml_node launcher) {
				_children.apply_launcher(name, launcher); });
		});
	});

	/* update query for blueprints of all unconfigured start nodes */
	if (_arch.valid()) {
		_depot_query_reporter.generate([&] (Xml_generator &xml) {
			xml.attribute("arch",    _arch);
			xml.attribute("version", _query_version.value);
			_children.gen_queries(xml);
		});
	}

	/*
	 * Apply blueprint after 'gen_queries'. Otherwise the application of a
	 * stale blueprint may flag children whose pkgs have been installed in the
	 * meanwhile as incomplete, suppressing their respective queries.
	 */
	try {
		Xml_node const blueprint = _blueprint_rom.xml();

		/* apply blueprint, except when stale */
		typedef String<32> Version;
		Version const version = blueprint.attribute_value("version", Version());
		if (version == Version(_query_version.value))
			_children.apply_blueprint(_blueprint_rom.xml());
	}
	catch (...) {
		error("spurious exception during deploy update (apply_blueprint)"); }

	/* feed missing packages to installation queue */
	if (!_installation.try_generate_manually_managed())
		_installation.generate([&] (Xml_generator &xml) {
			xml.attribute("arch", _arch);
			_children.gen_installation_entries(xml); });

	/* apply runtime condition checks */
	update_child_conditions();

	_dialog_generator.generate_dialog();
	_runtime_config_generator.generate_runtime_config();
}


void Sculpt::Deploy::gen_runtime_start_nodes(Xml_generator &xml) const
{
	/* depot-ROM instance for regular (immutable) depot content */
	xml.node("start", [&] () {
		gen_fs_rom_start_content(xml, "depot_rom", "cached_fs_rom", "depot",
		                         cached_depot_rom_state.ram_quota,
		                         cached_depot_rom_state.cap_quota); });

	/* depot-ROM instance for mutable content (/depot/local/) */
	xml.node("start", [&] () {
		gen_fs_rom_start_content(xml, "dynamic_depot_rom", "fs_rom", "depot",
		                         uncached_depot_rom_state.ram_quota,
		                         uncached_depot_rom_state.cap_quota); });

	xml.node("start", [&] () {
		gen_depot_query_start_content(xml); });

	Xml_node const manual_deploy = _manual_deploy_rom.xml();

	/* insert content of '<static>' node as is */
	if (manual_deploy.has_sub_node("static")) {
		Xml_node static_config = manual_deploy.sub_node("static");
		xml.append(static_config.content_base(), static_config.content_size());
	}

	/* generate start nodes for deployed packages */
	if (manual_deploy.has_sub_node("common_routes"))
		_children.gen_start_nodes(xml, manual_deploy.sub_node("common_routes"),
		                          "depot_rom", "dynamic_depot_rom");
}
