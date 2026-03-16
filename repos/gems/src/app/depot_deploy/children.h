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

/* Genode includes */
#include <util/list_model.h>
#include <base/service.h>
#include <depot/archive.h>

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

			return result;
		}

		Progress apply_launcher(Child::Launcher_name const &name, Node const &launcher)
		{
			Progress result = STALLED;
			_for_each_child([&] (Child &child) {
				if (child.apply_launcher(_alloc, name, launcher).progressed)
					result = PROGRESSED; });

			return result;
		}

		Progress apply_blueprint(Node const &blueprint)
		{
			Progress result = STALLED;

			blueprint.for_each_sub_node("pkg", [&] (Node const &pkg) {
				_for_each_child([&] (Child &child) {
					if (child.apply_blueprint(_alloc, pkg).progressed)
						result = PROGRESSED; }); });

			blueprint.for_each_sub_node("missing", [&] (Node const &missing) {
				_for_each_child([&] (Child &child) {
					if (child.mark_as_incomplete(missing).progressed)
						result = PROGRESSED; }); });

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

			return result;
		}

		Progress apply_condition(auto const &cond_fn)
		{
			Progress result = STALLED;
			_for_each_child([&] (Child &child) {
				if (child.apply_condition(cond_fn).progressed)
					result = PROGRESSED; });
			return result;
		}

		/**
		 * Call 'fn' with start 'Node' of each child that has an
		 * unsatisfied start condition.
		 */
		void for_each_unsatisfied_child(auto const &fn) const
		{
			_for_each_child([&] (Child const &child) {
				child.apply_if_unsatisfied(fn); });
		}

		void reset_incomplete()
		{
			_for_each_child([&] (Child &child) {
				child.reset_incomplete(); });
		}

		void gen_start_nodes(Generator &g, Node const &common,
		                     Child::Prio_levels prio_levels,
		                     Affinity::Space affinity_space,
		                     Child::Depot_rom_server const &default_depot_rom,
		                     auto const &cond_fn) const
		{
			_for_each_child([&] (Child const &child) {
				if (cond_fn(child.name))
					child.gen_start_node(g, _resource_types,
					                     common, prio_levels, affinity_space,
					                     default_depot_rom); });

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

		void gen_installation_entries(Generator &g) const
		{
			_for_each_child([&] (Child const &child) {
				child.gen_installation_entry(g); });
		}

		void for_each_missing_pkg_path(auto const &fn) const
		{
			_for_each_child([&] (Child const &child) {
				child.with_missing_pkg_path(fn); });
		}

		size_t count() const
		{
			size_t count = 0;
			_for_each_child([&] (Child const &) {
				++count; });
			return count;
		}

		bool any_incomplete() const
		{
			bool result = false;
			_for_each_child([&] (Child const &child) {
				result |= child.incomplete(); });
			return result;
		}

		void for_each_incomplete(auto const &fn) const
		{
			_for_each_child([&] (Child const &child) {
				if (child.incomplete())
					fn(child.name); });
		}

		bool any_blueprint_needed() const
		{
			bool result = false;
			_for_each_child([&] (Child const &child) {
				result |= child.blueprint_needed(); });
			return result;
		}

		bool exists(Child::Name const &name) const
		{
			return _child_dict.exists(name) || _alias_dict.exists(name);
		}

		bool blueprint_needed(Child::Name const &name) const
		{
			bool result = false;
			_for_each_child([&] (Child const &child) {
				if (child.name == name && child.blueprint_needed())
					result = true; });
			return result;
		}
};

#endif /* _CHILDREN_H_ */
