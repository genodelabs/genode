/*
 * \brief  GUI element that can be activated on clack
 * \author Norman Feske
 * \date   2018-05-03
 */

/*
 * Copyright (C) 2018 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _VIEW__ACTIVATABLE_ITEM_H_
#define _VIEW__ACTIVATABLE_ITEM_H_

#include "hoverable_item.h"

namespace Sculpt { struct Activatable_item; }


struct Sculpt::Activatable_item : Hoverable_item
{
	typedef Hoverable_item::Id Id;

	Id _selected  { };
	Id _activated { };

	/**
	 * Apply click - if item is hovered, the click selects the item but
	 * does not activate it yet
	 */
	void propose_activation_on_click()
	{
		_selected = _hovered;
	}

	void confirm_activation_on_clack()
	{
		if (_hovered.valid() && (_hovered == _selected))
			_activated = _selected;
	}

	void reset() { _selected = Id{}, _activated = Id{}; }

	/**
	 * Return true if item is currently activated
	 */
	bool activated(Id const &id) const { return id == _activated; }

	/**
	 * Generate button attributes depending on the item state
	 */
	void gen_button_attr(Xml_generator &xml, Id const &id) const
	{
		/* hover only as long as the button is not activated */
		if (!_selected.valid() || !_activated.valid())
			Hoverable_item::gen_button_attr(xml, id);

		if (_selected.valid() && _selected == _hovered)
			xml.attribute("selected", "yes");
	}
};

#endif /* _VIEW__ACTIVATABLE_ITEM_H_ */
