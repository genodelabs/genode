/*
 * \brief  State needed for traversing an index menu
 * \author Norman Feske
 * \date   2023-03-21
 */

/*
 * Copyright (C) 2023 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _MODEL__INDEX_MENU_H_
#define _MODEL__INDEX_MENU_H_

/* Genode includes */
#include <depot/archive.h>

/* local includes */
#include <types.h>

namespace Sculpt { struct Index_menu; }


struct Sculpt::Index_menu
{
	enum { MAX_LEVELS = 5 };

	unsigned _level = 0;

	using Name = String<64>;
	using User = Depot::Archive::User;

	Name _selected[MAX_LEVELS] { };

	void print(Output &out) const
	{
		using Genode::print;
		for (unsigned i = 0; i < _level; i++) {
			print(out, _selected[i]);
			if (i + 1 < _level)
				print(out, "  ");
		}
	}

	void _for_each_item(Xml_node const &index, auto const &fn, unsigned level) const
	{
		if (level == _level) {
			index.for_each_sub_node(fn);
			return;
		}

		index.for_each_sub_node("index", [&] (Xml_node const &index) {
			if (index.attribute_value("name", Name()) == _selected[level])
				_for_each_item(index, fn, level + 1); });
	}

	void for_each_item(Xml_node const &index, User const &user, auto const &fn) const
	{
		/*
		 * The index may contain duplicates, evaluate only the first match.
		 */
		bool first = true;
		index.for_each_sub_node("index", [&] (Xml_node const &index) {

			if (index.attribute_value("user", User()) != user)
				return;

			if (first)
				_for_each_item(index, fn, 0);

			first = false;
		});
	}

	unsigned level() const { return _level; }
};

#endif /* _MODEL__INDEX_MENU_H_ */
