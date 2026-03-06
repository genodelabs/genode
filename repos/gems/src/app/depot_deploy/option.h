/*
 * \brief  Deploy option
 * \author Norman Feske
 * \date   2026-03-05
 */

/*
 * Copyright (C) 2026 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _OPTION_H_
#define _OPTION_H_

/* Genode includes */
#include <util/list_model.h>
#include <base/attached_rom_dataspace.h>

/* local includes */
#include "child.h"

namespace Depot_deploy { class Option; }


struct Depot_deploy::Option : List_model<Option>::Element
{
	List_model<Child> children { };

	using Name = String<100>;

	Name const name;

	struct Action : Interface
	{
		virtual void deploy_option_changed(Name const &, Node const &) = 0;
	};

	struct Watched_rom
	{
		Action &_action;

		Name const &_name;

		Attached_rom_dataspace _ds;

		Signal_handler<Watched_rom> _handler;

		void _handle()
		{
			_ds.update();
			_action.deploy_option_changed(_name, _ds.node());
		}

		Watched_rom(Env &env, Name const &name, Action &action)
		:
			_action(action), _name(name),
			_ds(env, Name("option -> ", name).string()),
			_handler(env.ep(), *this, &Watched_rom::_handle)
		{
			_ds.sigh(_handler);
			_handler.local_submit();
		}
	};

	Constructible<Watched_rom> _watched_rom { };

	static Name node_name(Node const &node)
	{
		return node.attribute_value("name", Name());
	}

	Option(Name const &name) : name(name) { };

	void watch(Env &env, Action &action)
	{
		if (!_watched_rom.constructed())
			_watched_rom.construct(env, name, action);
	}

	/*
	 * \return true if config had any effect
	 */
	bool apply(Dictionary &dict, Allocator &alloc, Node const &option)
	{
		bool progress = false;

		children.update_from_node(option,

			/* create */
			[&] (Node const &node) -> Child & {
				progress = true;
				return *new (alloc)
					Child(dict, Child::node_name(node)); },

			/* destroy */
			[&] (Child &child) {
				progress = true;
				destroy(alloc, &child); },

			/* update */
			[&] (Child &child, Node const &node) {
				if (child.apply_config(alloc, node))
					progress = true; }
		);

		return progress;
	}

	/**
	 * List_model::Element
	 */
	bool matches(Node const &node) const { return node_name(node) == name; }

	/**
	 * List_model::Element
	 */
	static bool type_matches(Node const &node) { return node.has_type("option"); }
};

#endif /* _OPTION_H_ */
