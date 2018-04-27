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

	_runtime_config_generator.generate_runtime_config();
}


void Sculpt::Deploy::gen_runtime_start_nodes(Xml_generator &xml) const
{
	xml.node("start", [&] () {
		gen_fs_rom_start_content(xml, "depot_rom", "depot",
		                         depot_rom_state.ram_quota); });

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
		                          "depot_rom");
}
