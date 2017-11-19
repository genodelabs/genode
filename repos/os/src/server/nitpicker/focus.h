/*
 * \brief  Interfaces for requesting and controlling the focus
 * \author Norman Feske
 * \date   2017-11-17
 */

/*
 * Copyright (C) 2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _FOCUS_H_
#define _FOCUS_H_

#include "types.h"
#include "view_component.h"

namespace Nitpicker {
	struct Focus;
	struct Focus_controller;
}


/**
 * Interface used by the view stack
 */
class Nitpicker::Focus : Noncopyable
{
	private:

		View_owner const *_focused = nullptr;

	public:

		/**
		 * Return true if specified view owner has the input focus
		 */
		bool focused(View_owner const &owner) const
		{
			return &owner == _focused;
		}

		/**
		 * Return true if the specified view owner belongs to the same domain as
		 * the currently focused view owner
		 */
		bool same_domain_as_focused(View_owner const &owner) const
		{
			return owner.has_same_domain(_focused);
		}

		/**
		 * Return true if the specified view is the background view as defined for
		 * the currentlu focused view owner.
		 */
		bool focused_background(View_component const &view) const
		{
			return _focused && (_focused->background() == &view);
		}

		/**
		 * Set the input focus to the specified view owner
		 */
		void assign(View_owner const &focused) { _focused = &focused; }

		void reset() { _focused = nullptr; }

		void forget(View_owner const &owner)
		{
			if (_focused == &owner)
				_focused = nullptr;
		}
};


/**
 * Interface used by a nitpicker client to assign the focus to a session of
 * one of its child components (according to the session labels)
 */
struct Nitpicker::Focus_controller
{
	virtual void focus_view_owner(View_owner const &caller,
	                              View_owner &next_focused) = 0;
};

#endif /* _FOCUS_H_ */
