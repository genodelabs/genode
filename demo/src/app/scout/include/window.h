/*
 * \brief  Window interface
 * \author Norman Feske
 * \date   2006-08-30
 */

/*
 * Copyright (C) 2006-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _WINDOW_H_
#define _WINDOW_H_

#include "elements.h"
#include "platform.h"
#include "redraw_manager.h"


/**********************
 ** Window interface **
 **********************/

class Window : public Parent_element
{
	private:

		Platform       *_pf;
		int             _max_w;    /* max width of window  */
		int             _max_h;    /* max height of window */
		Redraw_manager *_redraw;   /* redraw manager       */

	public:

		Window(Platform *pf, Redraw_manager *redraw, int max_w, int max_h)
		:
			_pf(pf), _max_w(max_w), _max_h(max_h), _redraw(redraw)
		{
			/* init element attributes */
			_x = _y        = 0;
			_w             = pf->vw();
			_h             = pf->vh();
		}

		virtual ~Window() { }

		/**
		 * Return current window position
		 */
		virtual int view_x() { return _pf->vx(); }
		virtual int view_y() { return _pf->vy(); }
		virtual int view_w() { return _pf->vw(); }
		virtual int view_h() { return _pf->vh(); }

		/**
		 * Accessors
		 */
		Platform       *pf()     { return _pf; }
		int             max_w()  { return _max_w; }
		int             max_h()  { return _max_h; }
		Redraw_manager *redraw() { return _redraw; }

		/**
		 * Bring window to front
		 */
		virtual void top() { _pf->top_view(); }

		/**
		 * Move window to new position
		 */
		virtual void vpos(int x, int y) {
			_pf->view_geometry(x, y, _pf->vw(), _pf->vh(), 1, _pf->vbx(), _pf->vby()); }

		/**
		 * Define vertical scroll offset
		 */
		virtual void ypos(int ypos) { }
		virtual int  ypos() { return 0; }

		/**
		 * Format window
		 */
		virtual void format(int w, int h) { }

		/**
		 * Element interface
		 *
		 * This function just collects the specified regions to be
		 * redrawn but does not perform any immediate drawing
		 * operation. The actual drawing must be initiated by
		 * calling the process_redraw function.
		 */
		void redraw_area(int x, int y, int w, int h) {
			_redraw->request(x, y, w, h); }
};


/********************
 ** Event handlers **
 ********************/

class Drag_event_handler : public Event_handler
{
	protected:

		int _key_cnt;     /* number of curr. pressed keys */
		int _cmx, _cmy;   /* original mouse position      */
		int _omx, _omy;   /* current mouse positon        */

		virtual void start_drag() = 0;
		virtual void do_drag() = 0;

	public:

		/**
		 * Constructor
		 */
		Drag_event_handler() { _key_cnt = 0; }

		/**
		 * Event handler interface
		 */
		void handle(Event &ev)
		{
			if (ev.type == Event::PRESS)   _key_cnt++;
			if (ev.type == Event::RELEASE) _key_cnt--;

			if (_key_cnt == 0) return;

			/* first click starts dragging */
			if ((ev.type == Event::PRESS) && (_key_cnt == 1)) {
				_cmx = _omx = ev.mx;
				_cmy = _omy = ev.my;
				start_drag();
			}

			/* check if mouse was moved */
			if ((ev.mx == _cmx) && (ev.my == _cmy)) return;

			/* remember current mouse position */
			_cmx = ev.mx;
			_cmy = ev.my;

			do_drag();
		}
};


class Sizer_event_handler : public Drag_event_handler
{
	protected:

		Window *_window;
		int     _obw, _obh;   /* original window size */

		/**
		 * Event handler interface
		 */
		void start_drag()
		{
			_obw = _window->view_w();
			_obh = _window->view_h();
		}

		void do_drag()
		{
			/* calculate new window size */
			int nbw = _obw + _cmx - _omx;
			int nbh = _obh + _cmy - _omy;

			_window->format(nbw, nbh);
		}

	public:

		/**
		 * Constructor
		 */
		Sizer_event_handler(Window *window)
		{
			_window = window;
		}
};


class Mover_event_handler : public Drag_event_handler
{
	protected:

		Window *_window;
		int     _obx, _oby;    /* original launchpad position */

		void start_drag()
		{
			_obx = _window->view_x();
			_oby = _window->view_y();
			_window->top();
		}

		void do_drag()
		{
			int nbx = _obx + _cmx - _omx;
			int nby = _oby + _cmy - _omy;

			_window->vpos(nbx, nby);
		}

	public:

		/**
		 * Constructor
		 */
		Mover_event_handler(Window *window)
		{
			_window = window;
		}
};


#endif
