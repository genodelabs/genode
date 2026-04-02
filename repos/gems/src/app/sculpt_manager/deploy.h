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
#include <vfs.h>

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

	Managed_children::Dict _dict { };

	Managed_children _managed_children { };

	Enabled_options enabled_options { _alloc, _dict };

	static bool _present(auto const &registry, auto const &name)
	{
		bool result = false;
		registry.for_each([&] (auto const &elem) {
			if (name == elem) result = true; });
		return result;
	}

	Progress handle_deploy(Node const &deploy)
	{
		Progress result = STALLED;

		if (_managed_children.update_from_deploy_or_option(_alloc, _dict, deploy).progressed)
			result = PROGRESSED;

		if (enabled_options.update_from_deploy(deploy, _dict).progressed)
			result = PROGRESSED;

		return result;
	}

	Progress apply_option(Options::Name const &name, Node const &node)
	{
		return enabled_options.apply_option(name, _dict, node);
	}

	void watch_options(Vfs &vfs, Enabled_options::Action &action)
	{
		enabled_options.watch_options(vfs, action);
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

	void manage_resource_requests(Vfs &vfs, Runtime_state  const &state,
	                                        Runtime_config const &config) const
	{
		auto with_managed_attr = [&] (auto const &name, auto const &fn)
		{
			_dict.with_element(name,
				[&] (Managed_children::Child const &child) { fn(child.attr); },
				[&] { });
		};

		auto assign = [&] (auto const &option_name, auto const &child_name,
		                   size_t const ram, size_t const caps)
		{
			bool const option = (option_name.length() > 1);

			auto const path = option ? Path { "/model/option/", option_name }
			                         : Path { "/model/deploy" };

			Hid_edit::Query const query {
				option ? "option" : "deploy", " | + child ", child_name };

			vfs.edit(path, [&] (Hid_edit &edit) {
				if (ram)
					edit.adjust({ query, " | : ram" }, 0ul,
						[&] (size_t) { return ram; });
				if (caps)
					edit.adjust({ query, " | : caps" }, 0ul,
						[&] (size_t) { return caps; });
			});
		};

		state.for_each_resource_request(
			[&] (Start_name const &name, Runtime_state::Resource_request request) {
				with_managed_attr(name, [&] (Managed_children::Attr attr) {

					size_t ram = 0, caps = 0;

					if (request.ram.wanted() && request.ram.wanted() <= attr.max_ram)
						ram = request.ram.wanted();

					if (request.caps.wanted() && request.caps.wanted() <= attr.max_caps)
						caps = request.caps.wanted();

					if (!ram && !caps)
						return;

					config.with_component(name, [&] (Runtime_config::Component const &c) {
						assign(c.option, name, ram, caps); }, [&] { });
				});
			});
	}

	Deploy(Allocator &alloc,
	       Registry<Child_state> &child_states, Runtime_info const &runtime_info)
	:
		_alloc(alloc), _child_states(child_states), _runtime_info(runtime_info)
	{ }
};

#endif /* _DEPLOY_H_ */
