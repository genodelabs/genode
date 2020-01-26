/*
 * \brief  Menu-view dialog handling
 * \author Norman Feske
 * \date   2018-05-18
 */

/*
 * Copyright (C) 2018 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _VIEW__DIALOG_H_
#define _VIEW__DIALOG_H_

/* Genode includes */
#include <input/event.h>
#include <util/xml_generator.h>

/* local includes */
#include "types.h"
#include <view/hoverable_item.h>

namespace Sculpt { struct Dialog; }


struct Sculpt::Dialog : Interface
{
	/**
	 * Interface for triggering the (re-)generation of a menu-view dialog
	 *
	 * This interface ls implemented by a top-level dialog and called by a sub
	 * dialog.
	 */
	struct Generator : Interface { virtual void generate_dialog() = 0; };

	bool hovered = false;

	enum class Click_result { CONSUMED, IGNORED };
	enum class Clack_result { CONSUMED, IGNORED };

	using Hover_result = Hoverable_item::Hover_result;

	virtual void reset() = 0;

	virtual Hover_result hover(Xml_node hover) = 0;

	virtual void generate(Xml_generator &xml) const = 0;

	static Hover_result any_hover_changed(Hover_result head)
	{
		return head;
	}

	template <typename... TAIL>
	static Hover_result any_hover_changed(Hover_result head, TAIL... args)
	{
		if (head == Hover_result::CHANGED)
			return Hover_result::CHANGED;

		return any_hover_changed(args...);
	}

	Hover_result _match_sub_dialog(Xml_node node)
	{
		hovered = true;
		return hover(node);
	}

	template <typename... TAIL>
	Hover_result _match_sub_dialog(Xml_node hover, char const *sub_node,
	                                      TAIL &&... tail)
	{
		Hover_result result = Hover_result::UNMODIFIED;

		hover.with_sub_node(sub_node, [&] (Xml_node sub_hover) {
			if (_match_sub_dialog(sub_hover, tail...) == Hover_result::CHANGED)
				result = Hover_result::CHANGED; });

		return result;
	}

	template <typename... ARGS>
	Hover_result match_sub_dialog(ARGS &&... args)
	{
		bool const orig_hovered = hovered;

		hovered = false;

		Hover_result const result = _match_sub_dialog(args...);

		/* reset internal hover state of the dialog if no longer hovered */
		if (orig_hovered && !hovered)
			(void)this->hover(Xml_node("<empty/>"));

		return result;
	}

};

#endif /* _VIEW__DIALOG_H_ */
