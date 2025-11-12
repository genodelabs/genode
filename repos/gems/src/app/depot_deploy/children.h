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
#include "child.h"

namespace Depot_deploy { class Children; }


class Depot_deploy::Children
{
	private:

		Allocator &_alloc;

		List_model<Child> _children { };

		void _with_child(Child::Name const &name, auto const &fn) const
		{
			bool done = false;
			_children.for_each([&] (Child const &child) {
				if (!done && child.name() == name) {
					fn(child); done = true; } });
		}

	public:

		Children(Allocator &alloc) : _alloc(alloc) { }

		/*
		 * \return true if config had any effect
		 */
		bool apply_config(Node const &config)
		{
			bool progress = false;

			_children.update_from_node(config,

				/* create */
				[&] (Node const &node) -> Child & {
					progress = true;
					return *new (_alloc) Child(_alloc, node); },

				/* destroy */
				[&] (Child &child) {
					progress = true;
					destroy(_alloc, &child); },

				/* update */
				[&] (Child &child, Node const &node) {
					if (child.apply_config(node))
						progress = true; }
			);

			return progress;
		}

		/*
		 * \return true if launcher had any effect
		 */
		bool apply_launcher(Child::Launcher_name const &name, Node const &launcher)
		{
			bool any_child_changed = false;

			_children.for_each([&] (Child &child) {
				if (child.apply_launcher(name, launcher))
					any_child_changed = true; });

			return any_child_changed;
		}

		/*
		 * \return true if blueprint had an effect on any child
		 */
		bool apply_blueprint(Node const &blueprint)
		{
			bool any_child_changed = false;

			blueprint.for_each_sub_node("pkg", [&] (Node const &pkg) {
				_children.for_each([&] (Child &child) {
					if (child.apply_blueprint(pkg))
						any_child_changed = true; }); });

			blueprint.for_each_sub_node("missing", [&] (Node const &missing) {
				_children.for_each([&] (Child &child) {
					if (child.mark_as_incomplete(missing))
						any_child_changed = true; }); });

			return any_child_changed;
		}

		/*
		 * \return true if the condition of any child changed
		 */
		bool apply_condition(auto const &cond_fn)
		{
			bool any_condition_changed = false;
			_children.for_each([&] (Child &child) {
				any_condition_changed |= child.apply_condition(cond_fn); });
			return any_condition_changed;
		}

		/**
		 * Call 'fn' with start 'Node' of each child that has an
		 * unsatisfied start condition.
		 */
		void for_each_unsatisfied_child(auto const &fn) const
		{
			_children.for_each([&] (Child const &child) {
				child.apply_if_unsatisfied(fn); });
		}

		void reset_incomplete()
		{
			_children.for_each([&] (Child &child) {
				child.reset_incomplete(); });
		}

		void gen_start_nodes(Generator &g, Node const &common,
		                     Child::Prio_levels prio_levels,
		                     Affinity::Space affinity_space,
		                     Child::Depot_rom_server const &cached_depot_rom,
		                     Child::Depot_rom_server const &uncached_depot_rom,
		                     auto const &cond_fn) const
		{
			_children.for_each([&] (Child const &child) {
				if (cond_fn(child.name()))
					child.gen_start_node(g, common, prio_levels, affinity_space,
					                     cached_depot_rom, uncached_depot_rom); });
		}

		void gen_monitor_policy_nodes(Generator &g) const
		{
			_children.for_each([&] (Child const &child) {
				child.gen_monitor_policy_node(g); });
		}

		void gen_queries(Generator &g) const
		{
			_children.for_each([&] (Child const &child) {
				child.gen_query(g); });
		}

		void gen_installation_entries(Generator &g) const
		{
			_children.for_each([&] (Child const &child) {
				child.gen_installation_entry(g); });
		}

		void for_each_missing_pkg_path(auto const &fn) const
		{
			_children.for_each([&] (Child const &child) {
				child.with_missing_pkg_path(fn); });
		}

		size_t count() const
		{
			size_t count = 0;
			_children.for_each([&] (Child const &) {
				++count; });
			return count;
		}

		bool any_incomplete() const {

			bool result = false;
			_children.for_each([&] (Child const &child) {
				result |= child.incomplete(); });
			return result;
		}

		void for_each_incomplete(auto const &fn) const
		{
			_children.for_each([&] (Child const &child) {
				if (child.incomplete())
					fn(child.name()); });
		}

		bool any_blueprint_needed() const
		{
			bool result = false;
			_children.for_each([&] (Child const &child) {
				result |= child.blueprint_needed(); });
			return result;
		}

		bool exists(Child::Name const &name) const
		{
			bool result = false;
			_with_child(name, [&] (Child const &) { result = true; });
			return result;
		}

		bool blueprint_needed(Child::Name const &name) const
		{
			bool result = false;
			_children.for_each([&] (Child const &child) {
				if (child.name() == name && child.blueprint_needed())
					result = true; });
			return result;
		}
};

#endif /* _CHILDREN_H_ */
