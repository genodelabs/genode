/*
 * \brief   Scrollbar interface
 * \date    2005-10-24
 * \author  Norman Feske <norman.feske@genode-labs.com>
 */

/*
 * Copyright (C) 2005-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _SCROLLBAR_H_
#define _SCROLLBAR_H_

#include "widgets.h"
#include "fade_icon.h"

namespace Scout {
	class Scrollbar_listener;
	template <typename PT> class Scrollbar;
}


class Scout::Scrollbar_listener
{
	public:

		virtual ~Scrollbar_listener() { }

		/**
		 * Handle change of view position
		 */
		virtual void handle_scroll(int view_pos) = 0;
};


template <typename PT>
class Scout::Scrollbar : public Parent_element
{
	public:

		static const int sb_elem_w = 32;        /* scrollbar element width  */
		static const int sb_elem_h = 32;        /* scrollbar element height */

	private:

		Fade_icon<PT, 32, 32> _uparrow;
		Fade_icon<PT, 32, 32> _dnarrow;
		Fade_icon<PT, 32, 32> _slider;
		int                   _real_size;   /* size of content            */
		int                   _view_size;   /* size of viewport           */
		int                   _view_pos;    /* viewport position          */
		Scrollbar_listener   *_listener;    /* listener for scroll events */
		int                   _visibility;

		/**
		 * Utilities
		 */
		inline int _visible() { return _real_size > _view_size; }

	public:

		/**
		 * Constructor
		 */
		Scrollbar();

		/**
		 * Accessor functions
		 */
		int real_size   () { return _real_size; }
		int view_size   () { return _view_size; }
		int view_pos    () { return _view_pos;  }
		int slider_pos  ();
		int slider_size ();

		/**
		 * Set slider to specified position
		 */
		void slider_pos(int);

		/**
		 * Define scrollbar properties
		 */
		void view(int real_size, int view_size, int view_pos);

		/**
		 * Define listener to scroll events
		 */
		void listener(Scrollbar_listener *listener) { _listener = listener; }

		/**
		 * Notify listener about view port change
		 */
		void notify_listener();

		/**
		 * Set geometry of scrollbar and layout scrollbar elements
		 */
		void geometry(Rect);

		/**
		 * Element interface
		 */
		Element *find(Point);
};


#endif /* _SCROLLBAR_H_ */
