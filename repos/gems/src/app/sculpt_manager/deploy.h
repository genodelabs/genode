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
#include <model/options.h>
#include <vfs.h>

namespace Sculpt { struct Deploy; }


struct Sculpt::Deploy
{
	Allocator &_alloc;

	Managed_children::Dict _dict { };

	Managed_children _managed_children { };

	Enabled_options enabled_options { _alloc, _dict };

	Progress apply_deploy(Node const &deploy)
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

	static void assign_resources(Vfs &vfs, Runtime_config const &config,
	                             Start_name const &name,
	                             size_t const ram, size_t const caps)
	{
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

		config.with_component(name, [&] (Runtime_config::Component const &c) {
			assign(c.option, name, ram, caps); }, [&] { });
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

					assign_resources(vfs, config, name, ram, caps);
				});
			});
	}

	void reset(Vfs &vfs)
	{
		/* restart depot rom with quota reset to initial values */
		vfs.edit("/model/option/sculpt", [&] (Hid_edit &edit) {

			edit.adjust("option | + child depot_rom | : version",
			            0u, [&] (unsigned v) { return v + 1; });
			edit.adjust("option | + child depot_rom | : ram",
			            Num_bytes(), [&] (Num_bytes) { return 24*1024*1024; });

			edit.adjust("option | + child dynamic_depot_rom | : version",
			            0u, [&] (unsigned v) { return v + 1; });
			edit.adjust("option | + child dynamic_depot_rom | : ram",
			            Num_bytes(), [&] (Num_bytes) { return 8*1024*1024; });
		});
	}

	void reset_ram_fs(Vfs &vfs)
	{
		vfs.edit("/model/option/sculpt", [&] (Hid_edit &edit) {
			edit.adjust("option | + child ram_fs | : version",
			            0u, [&] (unsigned v) { return v + 1; });
			edit.adjust("option | + child ram_fs | : ram",
			            Num_bytes(), [&] (Num_bytes) { return 1*1024*1024; });
		});
	}

	Deploy(Allocator &alloc) : _alloc(alloc) { }
};

#endif /* _DEPLOY_H_ */
