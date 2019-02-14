/*
 * \brief  Window interface
 * \author Norman Feske
 * \date   2006-08-30
 */

/*
 * Copyright (C) 2006-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__SCOUT__WINDOW_H_
#define _INCLUDE__SCOUT__WINDOW_H_

#include <scout/misc_math.h>
#include <scout/parent_element.h>
#include <scout/platform.h>
#include <scout/graphics_backend.h>

namespace Scout {
	class Window;
	class Drag_event_handler;
	class Sizer_event_handler;
	class Mover_event_handler;
}


class Scout::Window : public Parent_element
{
	private:

		Graphics_backend &_gfx_backend;
		Rect              _dirty { };
		Area              _max_size;
		int               _request_cnt;  /* nb of requests since last process */
		bool const        _scout_quirk;  /* enable redraw quirk for scout     */

		/*
		 * We limit the rate of (expensive) view-position updates to the rate
		 * of 'process_redraw' calls instead of eagerly responding to
		 * individual input events (which trigger calls of 'vpos').
		 */
		Point _view_position, _next_view_position = _view_position;

		void _update_view_position()
		{
			if (_view_position == _next_view_position) return;

			_view_position = _next_view_position;
			_gfx_backend.position(_view_position);
		}

	public:

		Window(Graphics_backend &gfx_backend, Point position, Area size,
		       Area max_size, bool scout_quirk)
		:
			_gfx_backend(gfx_backend), _max_size(max_size), _request_cnt(0),
			_scout_quirk(scout_quirk), _view_position(position)
		{
			/* init element attributes */
			_size = size;
		}

		virtual ~Window() { }

		/**
		 * Return current window position
		 */
		int view_x() const { return _next_view_position.x(); }
		int view_y() const { return _next_view_position.y(); }
		int view_w() const { return _size.w(); }
		int view_h() const { return _size.h(); }

		Area max_size() const { return _max_size; }

		/**
		 * Bring window to front
		 */
		virtual void top() { _gfx_backend.bring_to_front(); }

		/**
		 * Move window to new position
		 */
		virtual void vpos(int x, int y) { _next_view_position = Point(x, y); }

		/**
		 * Define vertical scroll offset
		 */
		virtual void ypos(int) { }
		virtual int  ypos() { return 0; }

		/**
		 * Format window
		 */
		virtual void format(Area size)
		{
			_gfx_backend.view_area(size);
		}

		/**
		 * Element interface
		 *
		 * This function just collects the specified regions to be redrawn but
		 * does not perform any immediate drawing operation. The actual drawing
		 * must be initiated by calling the process_redraw function.
		 */
		void redraw_area(int x, int y, int w, int h) override
		{
			/*
			 * Scout redraw quirk
			 *
			 * Quick fix to avoid artifacts at the icon bar. The icon bar must
			 * always be drawn completely because of the interaction of the
			 * different layers.
			 */
			if (_scout_quirk && y < 64 + 32) {
				h = max(h + y, 64 + 32);
				w = _size.w();
				x = 0;
				y = 0;
			}

			Rect rect(Point(x, y), Area(w, h));

			/* first request since last process operation */
			if (_request_cnt == 0) {
				_dirty = rect;

			/* merge subsequencing requests */
			} else {
				_dirty = Rect::compound(_dirty, rect);
			}

			_request_cnt++;
		}

		/**
		 * Process redrawing operations
		 */
		void process_redraw()
		{
			_update_view_position();

			if (_request_cnt == 0) return;

			/* get actual drawing area (clipped against canvas dimensions) */
			int x1 = max(0, _dirty.x1());
			int y1 = max(0, _dirty.y1());
			int x2 = min((int)_size.w() - 1, _dirty.x2());
			int y2 = min((int)_size.h() - 1, _dirty.y2());

			if (x1 > x2 || y1 > y2) return;

			Canvas_base &canvas = _gfx_backend.back();
			canvas.clip(Rect(Point(x1, y1), Area(x2 - x1 + 1, y2 - y1 + 1)));

			/* draw into back buffer */
			try_draw(canvas, Point(0, 0));

			/*
			 * If we draw the whole area, we can flip the front
			 * and back buffers instead of copying pixels from the
			 * back to the front buffer.
			 */

			/* detemine if the whole area must be drawn */
			if (x1 == 0 && x2 == (int)_size.w() - 1
			 && y1 == 0 && y2 == (int)_size.h() - 1) {

				/* flip back end front buffers */
				_gfx_backend.swap_back_and_front();

			} else {
				_gfx_backend.copy_back_to_front(Rect(Point(x1, y1),
				                                     Area(x2 - x1 + 1, y2 - y1 + 1)));
			}

			/* reset request state */
			_request_cnt = 0;
		}
};


/********************
 ** Event handlers **
 ********************/

class Scout::Drag_event_handler : public Event_handler
{
	protected:

		int   _key_cnt = 0;     /* number of curr. pressed keys */
		Point _current_mouse_position { };
		Point _old_mouse_position     { };

		virtual void start_drag() = 0;
		virtual void do_drag() = 0;

	public:

		/**
		 * Constructor
		 */
		Drag_event_handler() { }

		/**
		 * Event handler interface
		 */
		void handle_event(Event const &ev) override
		{
			if (ev.type == Event::PRESS)   _key_cnt++;
			if (ev.type == Event::RELEASE) _key_cnt--;

			if (_key_cnt == 0) return;

			/* first click starts dragging */
			if ((ev.type == Event::PRESS) && (_key_cnt == 1)) {
				_current_mouse_position = _old_mouse_position = ev.mouse_position;
				start_drag();
			}

			/* check if mouse was moved */
			if ((ev.mouse_position.x() == _current_mouse_position.x())
			 && (ev.mouse_position.y() == _current_mouse_position.y()))
			 	return;

			/* remember current mouse position */
			_current_mouse_position = ev.mouse_position;

			do_drag();
		}
};


class Scout::Sizer_event_handler : public Drag_event_handler
{
	private:

		/*
		 * Noncopyable
		 */
		Sizer_event_handler(Sizer_event_handler const &);
		Sizer_event_handler &operator = (Sizer_event_handler const &);

	protected:

		Window *_window;
		int     _obw = 0, _obh = 0;   /* original window size */

		/**
		 * Event handler interface
		 */
		void start_drag() override
		{
			_obw = _window->view_w();
			_obh = _window->view_h();
		}

		void do_drag() override
		{
			/* calculate new window size */
			int nbw = _obw + _current_mouse_position.x() - _old_mouse_position.x();
			int nbh = _obh + _current_mouse_position.y() - _old_mouse_position.y();

			_window->format(Area(nbw, nbh));
		}

	public:

		/**
		 * Constructor
		 */
		Sizer_event_handler(Window *window) : _window(window) { }
};


class Scout::Mover_event_handler : public Drag_event_handler
{
	private:

		/*
		 * Noncopyable
		 */
		Mover_event_handler(Mover_event_handler const &);
		Mover_event_handler &operator = (Mover_event_handler const &);

	protected:

		Window *_window;
		int     _obx = 0, _oby = 0;    /* original launchpad position */

		void start_drag() override
		{
			_obx = _window->view_x();
			_oby = _window->view_y();
			_window->top();
		}

		void do_drag() override
		{
			int nbx = _obx + _current_mouse_position.x() - _old_mouse_position.x();
			int nby = _oby + _current_mouse_position.y() - _old_mouse_position.y();

			_window->vpos(nbx, nby);
		}

	public:

		/**
		 * Constructor
		 */
		Mover_event_handler(Window *window) : _window(window) { }
};

#endif /* _INCLUDE__SCOUT__WINDOW_H_ */
