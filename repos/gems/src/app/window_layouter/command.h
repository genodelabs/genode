/*
 * \brief  Command triggered via the keyboard
 * \author Norman Feske
 * \date   2016-02-01
 */

/*
 * Copyright (C) 2016-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _COMMAND_H_
#define _COMMAND_H_

#include <target.h>

namespace Window_layouter { class Command; }


struct Window_layouter::Command
{
	enum Type { NONE, NEXT_WINDOW, PREV_WINDOW, RAISE_WINDOW, TOGGLE_FULLSCREEN,
	            SCREEN, RELEASE_GRAB, PICK_UP, PLACE_DOWN,
	            DRAG, DROP, FREE_ARRANGE, STRICT_ARRANGE };

	Type         type;
	Target::Name target;

	static Command from_xml(Xml_node const &node)
	{
		auto from_string = [] (auto const &string) -> Type
		{
			if (string == "next_window")       return NEXT_WINDOW;
			if (string == "prev_window")       return PREV_WINDOW;
			if (string == "raise_window")      return RAISE_WINDOW;
			if (string == "toggle_fullscreen") return TOGGLE_FULLSCREEN;
			if (string == "screen")            return SCREEN;
			if (string == "release_grab")      return RELEASE_GRAB;
			if (string == "pick_up")           return PICK_UP;
			if (string == "place_down")        return PLACE_DOWN;
			if (string == "drag")              return DRAG;
			if (string == "drop")              return DROP;
			if (string == "free_arrange")      return FREE_ARRANGE;
			if (string == "strict_arrange")    return STRICT_ARRANGE;

			warning("cannot convert \"", string, "\" to action type");
			return NONE;
		};

		return {
			.type   = from_string(node.attribute_value("action", String<32>())),
			.target =             node.attribute_value("target", Name())
		};
	}
};

#endif /* _COMMAND_H_ */
