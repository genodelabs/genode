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
#include <util/xml_generator.h>
#include <base/service.h>
#include <depot/archive.h>

/* local includes */
#include "child.h"
#include <pointer.h>

namespace Depot_deploy { class Children; }


class Depot_deploy::Children
{
	private:

		Genode::Allocator                       &_alloc;
		Timer::Connection                       &_timer;
		Genode::Signal_context_capability const  _config_handler;
		Local::Const_pointer<Child>              _curr_child { };

		List_model<Child> _children { };

	public:

		struct No_match : Exception { };
		struct Finished : Exception { };

		Children(Genode::Allocator                       &alloc,
		         Timer::Connection                       &timer,
		         Genode::Signal_context_capability const  config_handler)
		:
			_alloc          { alloc },
			_timer          { timer },
			_config_handler { config_handler }
		{ }

		void apply_config(Xml_node config)
		{
			update_list_model_from_xml(_children, config,

				[&] (Xml_node const &node) -> Child & {
					return *new (_alloc)
						Child { _alloc, node, _timer, _config_handler }; },

				[&] (Child &child) { destroy(_alloc, &child); },

				[&] (Child &c, Xml_node const &node) { c.apply_config(node); }
			);
		}

		void apply_launcher(Child::Launcher_name const &name, Xml_node launcher)
		{
			_children.for_each([&] (Child &child) {
				child.apply_launcher(name, launcher); });
		}

		void apply_blueprint(Xml_node blueprint)
		{
			blueprint.for_each_sub_node("pkg", [&] (Xml_node pkg) {
				_children.for_each([&] (Child &child) {
					child.apply_blueprint(pkg); }); });

			blueprint.for_each_sub_node("missing", [&] (Xml_node missing) {
				_children.for_each([&] (Child &child) {
					child.mark_as_incomplete(missing); }); });
		}

		/*
		 * \return true if the condition of any child changed
		 */
		template <typename COND_FN>
		bool apply_condition(COND_FN const &fn)
		{
			bool any_condition_changed = false;
			_children.for_each([&] (Child &child) {
				any_condition_changed |= child.apply_condition(fn); });
			return any_condition_changed;
		}

		/**
		 * Call 'fn' with start 'Xml_node' of each child that has an
		 * unsatisfied start condition.
		 */
		template <typename FN>
		void for_each_unsatisfied_child(FN const &fn) const
		{
			_children.for_each([&] (Child const &child) {
				child.apply_if_unsatisfied(fn); });
		}

		void reset_incomplete()
		{
			_children.for_each([&] (Child &child) {
				child.reset_incomplete(); });
		}

		bool gen_start_nodes(Xml_generator &xml, Xml_node common,
		                     Child::Depot_rom_server const &cached_depot_rom,
		                     Child::Depot_rom_server const &uncached_depot_rom)
		{
			struct Break : Exception { };
			bool finished = true;
			try {
				_children.for_each([&] (Child &child) {

					child.gen_start_node(xml, common, cached_depot_rom, uncached_depot_rom);

					if (!child.finished()) {
						finished = false;
						throw Break();
					}
				});
			}
			catch (Break) { }
			return finished;
		}

		void print_conclusion()
		{
			_children.for_each([&] (Child &child) {
				child.print_conclusion();
			});
		}

		void conclusion(Result &result)
		{
			_children.for_each([&] (Child &child) {
				child.conclusion(result);
			});
		}

		void gen_queries(Xml_generator &xml)
		{
			try {
				Child const &child = _curr_child();
				if (child.finished()) {
					throw Local::Const_pointer<Child>::Invalid(); }

				child.gen_query(xml);
			}
			catch (Local::Const_pointer<Child>::Invalid) {
				struct Break : Exception { };
				try {
					_children.for_each([&] (Child const &child) {
						if (child.gen_query(xml)) {
							_curr_child = child;
							throw Break();
						}
					});
				} catch (Break) { }
			}
		}

		void gen_installation_entries(Xml_generator &xml) const
		{
			_children.for_each([&] (Child const &child) {
				child.gen_installation_entry(xml); });
		}

		bool any_incomplete() const {

			bool result = false;
			_children.for_each([&] (Child const &child) {
				result |= child.pkg_incomplete(); });
			return result;
		}

		bool exists(Child::Name const &name) const
		{
			bool result = false;
			_children.for_each([&] (Child const &child) {
				if (child.name() == name)
					result = true; });
			return result;
		}


		Child &find_by_name(Child::Name const &name)
		{
			Child *result = nullptr;
			_children.for_each([&] (Child const &child) {
				if (child.name() == name) {
					result = const_cast<Child *>(&child); }
			});
			if (!result) {
				throw No_match(); }

			return *result;
		}
};

#endif /* _CHILDREN_H_ */
