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
	private:

		bool _xray;
		bool _kill;

		/*
		 * Last clicked view. This view is receiving keyboard input, except
		 * for global keys.
		 */
		View const *_focused_view;

	public:

		Mode(): _xray(false), _kill(false), _focused_view(0) { }

		virtual ~Mode() { }

		/**
		 * Accessors
		 */
		bool xray() const { return _xray; }
		bool kill() const { return _kill; }
		bool flat() const { return !_xray && !_kill; }

		void leave_kill() { _kill = false; }
		void toggle_kill() { _kill = !_kill; }
		void toggle_xray() { _xray = !_xray; }

		View const *focused_view() const { return _focused_view; }

		void focused_view(View const *view) { _focused_view = view; }

		/**
		 * Discard all references to specified view
		 */
		virtual void forget(View const &v) {
			if (&v == _focused_view) _focused_view = 0; }
};

#endif
