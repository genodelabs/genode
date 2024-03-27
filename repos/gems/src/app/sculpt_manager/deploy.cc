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


void Sculpt::Deploy::view_diag(Scope<> &s) const
{
	/*
	 * Collect messages in registry, avoiding duplicates
	 */
	typedef String<64> Message;
	typedef Registered_no_delete<Message> Registered_message;
	Registry<Registered_message> messages { };

	auto gen_missing_dependencies = [&] (Xml_node start, Start_name const &name)
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

	_children.for_each_unsatisfied_child([&] (Xml_node start, Xml_node launcher,
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


void Sculpt::Deploy::_handle_managed_deploy(Xml_node const &managed_deploy)
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

		_launcher_listing_rom.with_xml([&] (Xml_node const &listing) {
			listing.for_each_sub_node("dir", [&] (Xml_node const &dir) {

				using Path = String<20>;
				Path const path = dir.attribute_value("path", Path());

				if (path != "/launcher")
					return;

				dir.for_each_sub_node("file", [&] (Xml_node const &file) {

					if (file.attribute_value("xml", false) == false)
						return;

					typedef Depot_deploy::Child::Launcher_name Name;
					Name const name = file.attribute_value("name", Name());

					file.for_each_sub_node("launcher", [&] (Xml_node const &launcher) {
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
			_blueprint_rom.with_xml([&] (Xml_node const &blueprint) {

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


void Sculpt::Deploy::gen_runtime_start_nodes(Xml_generator  &xml,
                                             Prio_levels     prio_levels,
                                             Affinity::Space affinity_space) const
{
	/* depot-ROM instance for regular (immutable) depot content */
	xml.node("start", [&] {
		gen_fs_rom_start_content(xml, "cached_fs_rom", "depot",
		                         cached_depot_rom_state); });

	/* depot-ROM instance for mutable content (/depot/local/) */
	xml.node("start", [&] {
		gen_fs_rom_start_content(xml, "fs_rom", "depot",
		                         uncached_depot_rom_state); });

	xml.node("start", [&] {
		gen_depot_query_start_content(xml); });

	_managed_deploy_rom.with_xml([&] (Xml_node const &managed_deploy) {

		/* insert content of '<static>' node as is */
		if (managed_deploy.has_sub_node("static")) {
			Xml_node static_config = managed_deploy.sub_node("static");
			static_config.with_raw_content([&] (char const *start, size_t length) {
				xml.append(start, length); });
		}

		/* generate start nodes for deployed packages */
		if (managed_deploy.has_sub_node("common_routes")) {
			_children.gen_start_nodes(xml, managed_deploy.sub_node("common_routes"),
			                          prio_levels, affinity_space,
			                          "depot_rom", "dynamic_depot_rom");
			xml.node("monitor", [&] {
				_children.gen_monitor_policy_nodes(xml);});
		}
	});
}
