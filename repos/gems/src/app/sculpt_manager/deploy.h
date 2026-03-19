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

/* local includes */
#include <types.h>
#include <runtime.h>
#include <model/options.h>

namespace Sculpt { struct Deploy; }


struct Sculpt::Deploy
{
	Allocator &_alloc;

	Registry<Child_state> &_child_states;

	Runtime_info const &_runtime_info;

	Child_state cached_depot_rom_state {
		_child_states, { .name      = "depot_rom",
		                 .binary    = "cached_fs_rom",
		                 .priority  = Priority::STORAGE,
		                 .location  = { },
		                 .initial   = { Ram_quota{24*1024*1024}, Cap_quota{200} },
		                 .max       = { Ram_quota{2*1024*1024*1024UL}, { } } } };

	Child_state uncached_depot_rom_state {
		_child_states, { .name      = "dynamic_depot_rom",
		                 .binary    = "fs_rom",
		                 .priority  = Priority::STORAGE,
		                 .location  = { },
		                 .initial   = { Ram_quota{8*1024*1024}, Cap_quota{200} },
		                 .max       = { Ram_quota{2*1024*1024*1024UL}, { } } } };

	Enabled_options enabled_options { _alloc };

	using Option_to_remove = Registered_no_delete<Options::Name>;
	using Option_to_add    = Registered_no_delete<Options::Name>;

	Registry<Option_to_remove> _options_to_remove { };
	Registry<Option_to_add>    _options_to_add    { };

	static bool _present(auto const &registry, auto const &name)
	{
		bool result = false;
		registry.for_each([&] (auto const &elem) {
			if (name == elem) result = true; });
		return result;
	}

	void enable_option(Options::Name const &name)
	{
		if (!_present(_options_to_add, name))
			new (_alloc) Option_to_add(_options_to_add, name);
	}

	void disable_option(Options::Name const &name)
	{
		if (!_present(_options_to_remove, name))
			new (_alloc) Option_to_remove(_options_to_remove, name);
	}

	void _reset_interactive_option_changes()
	{
		_options_to_add   .for_each([&] (auto &elem) { destroy(_alloc, &elem); });
		_options_to_remove.for_each([&] (auto &elem) { destroy(_alloc, &elem); });
	}

	void handle_deploy_config(Generator &g, Node const &deploy)
	{
		/*
		 * Ignore intermediate states that may occur when manually updating
		 * the config/deploy configuration. Depending on the tool used,
		 * the original file may be unlinked before the new version is
		 * created. The temporary empty configuration must not be applied.
		 */
		if (deploy.type() == "empty")
			return;

		Progress progress = enabled_options.update_from_deploy(deploy);
		(void)progress;

		/*
		 * Copy the <start> node from manual deploy config, unless the
		 * component was interactively killed by the user.
		 */
		deploy.for_each_sub_node([&] (Node const &node) {

			if (node.type() == "start") {
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
			}

			if (node.type() == "option") {
				auto const name = node.attribute_value("name", Options::Name());
				if (!_present(_options_to_remove, name))
					(void)g.append_node(node, Generator::Max_depth { 2 });
			}
		});

		/*
		 * Supplement interactively added options
		 */
		_options_to_add.for_each([&] (Option_to_add const &name) {
			if (!enabled_options.exists(name))
				gen_named_node(g, "option", name); });

		/*
		 * Add start nodes for interactively launched components
		 */
		_runtime_info.gen_launched_deploy_start_nodes(g);

		_reset_interactive_option_changes();
	}

	void gen_child_nodes(Generator &g) const
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

	Deploy(Allocator &alloc,
	       Registry<Child_state> &child_states, Runtime_info const &runtime_info)
	:
		_alloc(alloc), _child_states(child_states), _runtime_info(runtime_info)
	{ }
};

#endif /* _DEPLOY_H_ */
