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

#ifndef _DEPLOY_H_
#define _DEPLOY_H_

/* Genode includes */
#include <base/attached_rom_dataspace.h>
#include <os/reporter.h>

/* included from depot_deploy tool */
#include <children.h>

/* local includes */
#include <model/launchers.h>
#include <types.h>
#include <runtime.h>
#include <managed_config.h>
#include <view/dialog.h>

namespace Sculpt { struct Deploy; }


struct Sculpt::Deploy
{
	Env &_env;

	Allocator &_alloc;

	Runtime_info const &_runtime_info;

	Dialog::Generator &_dialog_generator;

	Runtime_config_generator &_runtime_config_generator;

	Attached_rom_dataspace const &_launcher_listing_rom;

	typedef String<16> Arch;
	Arch _arch { };

	struct Query_version { unsigned value; } _query_version { 0 };

	Child_state cached_depot_rom_state {
		"depot_rom", Ram_quota{24*1024*1024}, Cap_quota{200} };

	Child_state uncached_depot_rom_state {
		"dynamic_depot_rom", Ram_quota{8*1024*1024}, Cap_quota{200} };

	Attached_rom_dataspace _blueprint_rom { _env, "report -> runtime/depot_query/blueprint" };

	Expanding_reporter _depot_query_reporter { _env, "query", "depot_query"};

	/*
	 * Report written to '/config/managed/deploy'
	 *
	 * This report takes the manually maintained '/config/deploy' and the
	 * interactive state as input.
	 */
	Expanding_reporter _managed_deploy_config { _env, "config", "deploy_config" };

	/* config obtained from '/config/managed/deploy' */
	Attached_rom_dataspace _managed_deploy_rom { _env, "config -> managed/deploy" };

	void update_managed_deploy_config(Xml_node deploy)
	{
		_managed_deploy_config.generate([&] (Xml_generator &xml) {

			Arch const arch = deploy.attribute_value("arch", Arch());
			if (arch.valid())
				xml.attribute("arch", arch);

			auto append_xml_node = [&] (Xml_node node) {
				xml.append("\t");
				xml.append(node.addr(), node.size());
				xml.append("\n");
			};

			/* copy <common_routes> from manual deploy config */
			deploy.for_each_sub_node("common_routes", [&] (Xml_node node) {
				append_xml_node(node); });

			/*
			 * Copy the <start> node from manual deploy config, unless the
			 * component was interactively killed by the user.
			 */
			deploy.for_each_sub_node("start", [&] (Xml_node node) {
				Start_name const name = node.attribute_value("name", Start_name());
				if (!_runtime_info.abandoned_by_user(name))
					append_xml_node(node);
			});

			/*
			 * Add start nodes for interactively launched components.
			 */
			_runtime_info.gen_launched_deploy_start_nodes(xml);
		});
	}

	bool _manual_installation_scheduled = false;

	Managed_config<Deploy> _installation {
		_env, "installation", "installation", *this, &Deploy::_handle_installation };

	void _handle_installation(Xml_node manual_config)
	{
		_manual_installation_scheduled = manual_config.has_sub_node("archive");
		handle_deploy();
	}

	Depot_deploy::Children _children { _alloc };

	bool update_needed() const
	{
		return _manual_installation_scheduled || _children.any_incomplete();
	}

	void handle_deploy();

	void _handle_managed_deploy()
	{
		_managed_deploy_rom.update();
		_query_version.value++;
		handle_deploy();
	}

	void _handle_blueprint()
	{
		_blueprint_rom.update();
		handle_deploy();
	}

	/**
	 * Call 'fn' for each unsatisfied dependency of the child's 'start' node
	 */
	template <typename FN>
	void _for_each_missing_server(Xml_node start, FN const &fn) const
	{
		start.for_each_sub_node("route", [&] (Xml_node route) {
			route.for_each_sub_node("service", [&] (Xml_node service) {
				service.for_each_sub_node("child", [&] (Xml_node child) {
					Start_name const name = child.attribute_value("name", Start_name());

					/*
					 * The dependency to the default-fs alias is always
					 * satisfied during the deploy phase. But it does not
					 * appear in the runtime-state report.
					 */
					if (name == "default_fs_rw")
						return;

					if (!_runtime_info.present_in_runtime(name))
						fn(name);
				});
			});
		});
	}

	/**
	 * Re-evaluate child dependencies
	 *
	 * \return true if any condition has changed and new children may have
	 *              become able to start
	 */
	bool update_child_conditions();

	void _gen_missing_dependencies(Xml_generator &, Start_name const &, Xml_node, int &) const;

	void gen_child_diagnostics(Xml_generator &xml) const;

	void gen_runtime_start_nodes(Xml_generator &) const;

	Signal_handler<Deploy> _managed_deploy_handler {
		_env.ep(), *this, &Deploy::_handle_managed_deploy };

	Signal_handler<Deploy> _blueprint_handler {
		_env.ep(), *this, &Deploy::_handle_blueprint };

	void restart()
	{
		/* ignore stale query results */
		_query_version.value++;

		_children.apply_config(Xml_node("<config/>"));
	}

	void reattempt_after_installation()
	{
		_children.reset_incomplete();
		handle_deploy();
	}

	Deploy(Env &env, Allocator &alloc, Runtime_info const &runtime_info,
	       Dialog::Generator &dialog_generator,
	       Runtime_config_generator &runtime_config_generator,
	       Attached_rom_dataspace const &launcher_listing_rom)
	:
		_env(env), _alloc(alloc), _runtime_info(runtime_info),
		_dialog_generator(dialog_generator),
		_runtime_config_generator(runtime_config_generator),
		_launcher_listing_rom(launcher_listing_rom)
	{
		_managed_deploy_rom.sigh(_managed_deploy_handler);
		_blueprint_rom.sigh(_blueprint_handler);
	}
};

#endif /* _DEPLOY_H_ */
