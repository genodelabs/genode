/*
 * \brief  Action triggered by the user
 * \author Norman Feske
 * \date   2016-02-01
 */

/*
 * Copyright (C) 2016-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _ACTION_H_
#define _ACTION_H_

#include <target.h>

namespace Window_layouter { class Action; }


/**
 * Result of the application of a key event to the key-sequence tracker
 */
class Window_layouter::Action
{
	public:

		enum Type {
			NONE,
			NEXT_WINDOW,
			PREV_WINDOW,
			RAISE_WINDOW,
			TOGGLE_FULLSCREEN,
			CLOSE,
			NEXT_WORKSPACE,
			PREV_WORKSPACE,
			MARK,
			DETACH,
			ATTACH,
			COLUMN,
			ROW,
			REMOVE,
			NEXT_COLUMN,
			PREV_COLUMN,
			NEXT_ROW,
			PREV_ROW,
			NEXT_TAB,
			PREV_TAB,
			TOOGLE_OVERLAY,
			SCREEN,
		};

	private:

		Type         _type;
		Target::Name _target;

		template <Genode::size_t N>
		static Type _type_by_string(String<N> const &string)
		{
			if (string == "next_window")       return NEXT_WINDOW;
			if (string == "prev_window")       return PREV_WINDOW;
			if (string == "raise_window")      return RAISE_WINDOW;
			if (string == "toggle_fullscreen") return TOGGLE_FULLSCREEN;
			if (string == "screen")            return SCREEN;

			Genode::warning("cannot convert \"", string, "\" to action type");
			return NONE;
		}

	public:

		Action(Type type) : _type(type), _target() { }

		template <Genode::size_t N>
		Action(String<N> const &string, Target::Name const &arg)
		: _type(_type_by_string(string)), _target(arg) { }

		Type type() const { return _type; }
		Target::Name target_name() const { return _target; }
};

#endif /* _ACTION_H_ */
