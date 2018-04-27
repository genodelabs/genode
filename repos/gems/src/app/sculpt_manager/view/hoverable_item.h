/*
 * \brief  GUI element that shows hovering feedback
 * \author Norman Feske
 * \date   2018-05-03
 */

/*
 * Copyright (C) 2018 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _VIEW__HOVERABLE_ITEM_H_
#define _VIEW__HOVERABLE_ITEM_H_

#include "types.h"
#include "xml.h"

namespace Sculpt { struct Hoverable_item; }


struct Sculpt::Hoverable_item
{
	typedef String<64> Id;

	Id _hovered  { };

	/**
	 * Set ID to matching hover sub node name
	 *
	 * \return true if hovering changed
	 */
	template <typename... ARGS>
	bool match(Xml_node hover, ARGS &&... args)
	{
		Id const orig = _hovered;
		_hovered = query_attribute<Id>(hover, args...);
		return _hovered != orig;
	}

	/**
	 * Return true if item is currently hovered
	 */
	bool hovered(Id const &id) const { return id == _hovered; }

	/**
	 * Generate button attributes depending on the item state
	 */
	void gen_button_attr(Xml_generator &xml, Id const &id) const
	{
		xml.attribute("name", id);

		if (hovered(id))  xml.attribute("hovered", "yes");
	}
};

#endif /* _VIEW__HOVERABLE_ITEM_H_ */
