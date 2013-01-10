/*
 * \brief  Nitpicker mode
 * \author Norman Feske
 * \date   2006-08-22
 */

/*
 * Copyright (C) 2006-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _MODE_H_
#define _MODE_H_

class View;

class Mode
{
	protected:

		unsigned _mode;

		/*
		 * Last clicked view. This view is receiving
		 * keyboard input.
		 */
		View *_focused_view;

	public:

		/*
		 * Bitmasks for the different Nitpicker modes
		 */
		enum {
			XRAY = 0x1,
			KILL = 0x2,
		};

		Mode(): _mode(0), _focused_view(0) { }

		virtual ~Mode() { }

		/**
		 * Accessors
		 */
		bool xray() { return _mode & XRAY; }
		bool kill() { return _mode & KILL; }
		bool flat() { return _mode == 0; }

		View *focused_view() { return _focused_view; }

		/**
		 * Discard all references to specified view
		 */
		virtual void forget(View *v) {
			if (v == _focused_view) _focused_view = 0; }
};

#endif
