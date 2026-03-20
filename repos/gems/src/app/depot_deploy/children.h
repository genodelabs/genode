/*
 * \brief  State tracking of subsystems deployed from depot packages
 * \author Norman Feske
 * \date   2018-01-23
 */

/*
 * Copyright (C) 2018 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _CHILDREN_H_
#define _CHILDREN_H_

/* local includes */
#include "option.h"

namespace Depot_deploy { class Children; }


class Depot_deploy::Children
{
	private:

		Allocator &_alloc;

		List_model<Child>  _immediate_children { };
		List_model<Alias>  _immediate_aliases { };
		List_model<Option> _options { };

		/**
		 * Dictionary of all immediate children and the children of all options
		 */
		Child_dict _child_dict { };
		Alias_dict _alias_dict { };

		void _for_each_child(auto const &fn)
		{
			auto unique_fn = [&] (Child &c) { if (!c.duplicate) fn(c); };

			_immediate_children.for_each([&] (Child &c) { unique_fn(c); });

			_options.for_each([&] (Option &option) {
				option.children.for_each([&] (Child &c) { unique_fn(c); }); });
		}

		void _for_each_child(auto const &fn) const
		{
			auto unique_fn = [&] (Child const &c) { if (!c.duplicate) fn(c); };

			_immediate_children.for_each([&] (Child const &c) { unique_fn(c); });

			_options.for_each([&] (Option const &option) {
				option.children.for_each([&] (Child const &c) { unique_fn(c); }); });
		}

		void _for_each_alias(auto const &fn) const
		{
			_immediate_aliases.for_each([&] (Alias const &a) { fn(a); });

			_options.for_each([&] (Option const &option) {
				option.aliases.for_each([&] (Alias const &a) { fn(a); }); });
		}

		Resource::Types const _resource_types { };

		void _update_deps()
		{
			_for_each_child([&] (Child &child) { child.reset_deps(); });

			for (;;) { /* iterate until all inter-dependencies are settled */
				Progress progress = STALLED;
				_for_each_child([&] (Child &child) {
					if (child.update_deps(_child_dict, _alias_dict).progressed)
						progress = PROGRESSED; });
				if (progress.stalled())
					break;
			}
		}

	public:

		Children(Allocator &alloc) : _alloc(alloc) { }

		Progress apply_deploy(Node const &deploy)
		{
			Progress result = STALLED;

			if (Child::update_from_node(_immediate_children, _child_dict, _alloc, deploy).progressed)
				result = PROGRESSED;

			if (Alias::update_from_node(_immediate_aliases, _alias_dict, _alloc, deploy).progressed)
				result = PROGRESSED;

			_options.update_from_node(deploy,

				/* create */
				[&] (Node const &node) -> Option & {
					result = PROGRESSED;
					return *new (_alloc) Option(Option::node_name(node)); },

				/* destroy */
				[&] (Option &option) {
					result = PROGRESSED;
					option.apply(_child_dict, _alias_dict, _alloc, Node()); /* flush children */
					destroy(_alloc, &option); },

				/* update */
				[&] (Option &, Node const &) { }
			);

			if (result.progressed) _update_deps();

			return result;
		}

		Progress apply_blueprint(Node const &blueprint)
		{
			Progress result = STALLED;

			blueprint.for_each_sub_node([&] (Node const &pkg) {
				_for_each_child([&] (Child &child) {
					if (child.apply_blueprint(_alloc, pkg).progressed)
						result = PROGRESSED; }); });

			if (result.progressed) _update_deps();

			return result;
		}

		/**
		 * Register 'action' handler for option updates
		 *
		 * This method should be called whenever 'apply_config' has indictated
		 * any change (progress). New options may have appeared that are worth
		 * watching.
		 */
		void watch_options(Env &env, Option::Action &action)
		{
			_options.for_each([&] (Option &option) { option.watch(env, action); });
		}

		/**
		 * Supply the config for option of the given name
		 */
		Progress apply_option(Option::Name const &name, Node const &config)
		{
			Progress result = STALLED;
			_options.for_each([&] (Option &option) {
				if (option.name == name)
					result = option.apply(_child_dict, _alias_dict, _alloc, config); });

			if (result.progressed) _update_deps();

			return result;
		}

		void rediscover_blueprints()
		{
			_for_each_child([&] (Child &child) { child.rediscover_blueprint(); });
		}

		void gen_start_nodes(Generator &g,
		                     Node             const &common,
		                     Prio_levels      const prio_levels,
		                     Affinity::Space  const affinity_space,
		                     Depot_rom_server const &default_depot_rom,
		                     auto             const &cond_fn) const
		{
			Global const global {
				.resource_types    = _resource_types,
				.child_dict        = _child_dict,
				.alias_dict        = _alias_dict,
				.common            = common,
				.prio_levels       = prio_levels,
				.affinity_space    = affinity_space,
				.default_depot_rom = default_depot_rom
			};

			_immediate_children.for_each([&] (Child const &child) {
				if (!child.duplicate && cond_fn(child.name))
					child.gen_start_node(g, Option_name { }, global); });

			_options.for_each([&] (Option const &option) {
				option.children.for_each([&] (Child const &child) {
					if (!child.duplicate && cond_fn(child.name))
						child.gen_start_node(g, option.name, global); }); });

			_for_each_alias([&] (Alias const &alias) {
				g.node("alias", [&] {
					g.attribute("name",  alias.name);
					g.attribute("child", alias.to_child); }); });
		}

		void gen_monitor_policy_nodes(Generator &g) const
		{
			_for_each_child([&] (Child const &child) {
				child.gen_monitor_policy_node(g); });
		}

		void gen_queries(Generator &g) const
		{
			_for_each_child([&] (Child const &child) {
				child.gen_query(g); });
		}
};

#endif /* _CHILDREN_H_ */
