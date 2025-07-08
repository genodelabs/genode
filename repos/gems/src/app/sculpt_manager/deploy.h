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
#include <model/download_queue.h>
#include <types.h>
#include <runtime.h>
#include <managed_config.h>
#include <view/dialog.h>
#include <depot_query.h>

namespace Sculpt { struct Deploy; }


struct Sculpt::Deploy
{
	using Prio_levels = Depot_deploy::Child::Prio_levels;

	Env &_env;

	Allocator &_alloc;

	Registry<Child_state> &_child_states;

	Runtime_info const &_runtime_info;

	struct Action : Interface { virtual void refresh_deploy_dialog() = 0; };

	Action &_action;

	Runtime_config_generator &_runtime_config_generator;

	Depot_query &_depot_query;

	Rom_data const &_launcher_listing_rom;
	Rom_data const &_blueprint_rom;

	Download_queue &_download_queue;

	using Arch = String<16>;
	Arch _arch { };

	Child_state cached_depot_rom_state {
		_child_states, { .name      = "depot_rom",
		                 .priority  = Priority::STORAGE,
		                 .cpu_quota = 0,
		                 .location  = { },
		                 .initial   = { Ram_quota{24*1024*1024}, Cap_quota{200} },
		                 .max       = { Ram_quota{2*1024*1024*1024UL}, { } } } };

	Child_state uncached_depot_rom_state {
		_child_states, { .name      = "dynamic_depot_rom",
		                 .priority  = Priority::STORAGE,
		                 .cpu_quota = 0,
		                 .location  = { },
		                 .initial   = { Ram_quota{8*1024*1024}, Cap_quota{200} },
		                 .max       = { Ram_quota{2*1024*1024*1024UL}, { } } } };

	/*
	 * Report written to '/config/managed/deploy'
	 *
	 * This report takes the manually maintained '/config/deploy' and the
	 * interactive state as input.
	 */
	Expanding_reporter _managed_deploy_config { _env, "config", "deploy_config" };

	/* config obtained from '/config/managed/deploy' */
	Rom_handler<Deploy> _managed_deploy_rom {
		_env, "config -> managed/deploy", *this, &Deploy::_handle_managed_deploy };

	Constructible<Buffered_node> _template { };

	void use_as_deploy_template(Node const &deploy)
	{
		_template.construct(_alloc, deploy);
	}

	void update_managed_deploy_config()
	{
		if (!_template.constructed())
			return;

		Node const &deploy = *_template;

		if (deploy.type() == "empty")
			return;

		_update_managed_deploy_config(deploy);
	}

	void _update_managed_deploy_config(Node const &deploy)
	{
		/*
		 * Ignore intermediate states that may occur when manually updating
		 * the config/deploy configuration. Depending on the tool used,
		 * the original file may be unlinked before the new version is
		 * created. The temporary empty configuration must not be applied.
		 */
		if (deploy.type() == "empty")
			return;

		_managed_deploy_config.generate([&] (Generator &g) {

			Arch const arch = deploy.attribute_value("arch", Arch());
			if (arch.valid())
				g.attribute("arch", arch);

			/* copy <common_routes> from manual deploy config */
			deploy.for_each_sub_node("common_routes", [&] (Node const &node) {
				(void)g.append_node(node, Generator::Max_depth { 10 }); });

			/*
			 * Copy the <start> node from manual deploy config, unless the
			 * component was interactively killed by the user.
			 */
			deploy.for_each_sub_node("start", [&] (Node const &node) {
				Start_name const name = node.attribute_value("name", Start_name());
				if (_runtime_info.abandoned_by_user(name))
					return;

				g.node("start", [&] {

					/*
					 * Copy attributes
					 */

					g.attribute("name", name);

					/* override version with restarted version, after restart */
					using Version = Child_state::Version;
					Version version = _runtime_info.restarted_version(name);
					if (version.value == 0)
						version = Version { node.attribute_value("version", 0U) };

					if (version.value > 0)
						g.attribute("version", version.value);

					auto copy_attribute = [&] (auto attr)
					{
						if (node.has_attribute(attr)) {
							using Value = String<128>;
							g.attribute(attr, node.attribute_value(attr, Value()));
						}
					};

					copy_attribute("caps");
					copy_attribute("ram");
					copy_attribute("cpu");
					copy_attribute("priority");
					copy_attribute("pkg");
					copy_attribute("managing_system");

					/* copy start-node content */
					if (!g.append_node_content(node, Generator::Max_depth { 20 }))
						warning("start node too deeply nested: ", node);
				});
			});

			/*
			 * Add start nodes for interactively launched components.
			 */
			_runtime_info.gen_launched_deploy_start_nodes(g);
		});
	}

	bool _manual_installation_scheduled = false;

	Managed_config<Deploy> _installation {
		_env, "installation", "installation", *this, &Deploy::_handle_installation };

	void _handle_installation(Node const &manual_config)
	{
		_manual_installation_scheduled = manual_config.has_sub_node("archive");
		handle_deploy();
	}

	Depot_deploy::Children _children { _alloc };

	bool update_needed() const
	{
		return _manual_installation_scheduled
		    || _download_queue.any_active_download();
	}

	void _handle_managed_deploy(Node const &);

	void handle_deploy()
	{
		_managed_deploy_rom.with_node([&] (Node const &managed_deploy) {
			_handle_managed_deploy(managed_deploy); });
	}

	/**
	 * Call 'fn' for each unsatisfied dependency of the child's 'start' node
	 */
	void _for_each_missing_server(Node const &start, auto const &fn) const
	{
		start.for_each_sub_node("route", [&] (Node const &route) {
			route.for_each_sub_node("service", [&] (Node const &service) {
				service.for_each_sub_node("child", [&] (Node const &child) {
					Start_name const name = child.attribute_value("name", Start_name());

					/*
					 * The dependency to the default-fs alias is always
					 * satisfied during the deploy phase. But it does not
					 * appear in the runtime-state report.
					 */
					if (name == "default_fs_rw")
						return;

					if (!_runtime_info.present_in_runtime(name) || _children.blueprint_needed(name))
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

	bool any_unsatisfied_child() const
	{
		bool all_satisfied = true;
		_children.for_each_unsatisfied_child(
			[&] (Node const &, Node const &, Start_name const &) {
				all_satisfied = false; });
		return !all_satisfied;
	}

	void view_diag(Scope<> &) const;

	void gen_runtime_start_nodes(Generator &, Prio_levels, Affinity::Space) const;

	void restart()
	{
		cached_depot_rom_state  .trigger_restart();
		uncached_depot_rom_state.trigger_restart();

		/* ignore stale query results */
		_depot_query.trigger_depot_query();

		_children.apply_config(Node());
	}

	void reattempt_after_installation()
	{
		_children.reset_incomplete();
		handle_deploy();
	}

	void gen_depot_query(Generator &g) const
	{
		_children.gen_queries(g);
	}

	void update_installation()
	{
		/* feed missing packages to installation queue */
		if (_installation.try_generate_manually_managed())
			return;

		_children.for_each_missing_pkg_path([&] (Depot::Archive::Path const path) {
			_download_queue.add(path, Verify { true }); });

		_installation.generate([&] (Generator &g) {
			g.attribute("arch", _arch);
			_download_queue.gen_installation_entries(g);
		});
	}

	Deploy(Env &env, Allocator &alloc, Registry<Child_state> &child_states,
	       Runtime_info const &runtime_info,
	       Action &action,
	       Runtime_config_generator &runtime_config_generator,
	       Depot_query &depot_query,
	       Rom_data const &launcher_listing_rom,
	       Rom_data const &blueprint_rom,
	       Download_queue &download_queue)
	:
		_env(env), _alloc(alloc), _child_states(child_states),
		_runtime_info(runtime_info),
		_action(action),
		_runtime_config_generator(runtime_config_generator),
		_depot_query(depot_query),
		_launcher_listing_rom(launcher_listing_rom),
		_blueprint_rom(blueprint_rom),
		_download_queue(download_queue)
	{ }
};

#endif /* _DEPLOY_H_ */
