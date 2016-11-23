/*
 * \brief  Nitpicker-based logging service
 * \author Norman Feske
 * \date   2006-09-18
 */

/*
 * Copyright (C) 2006-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#include <util/arg_string.h>
#include <base/env.h>
#include <base/sleep.h>
#include <base/lock.h>
#include <base/heap.h>
#include <base/snprintf.h>
#include <base/rpc_server.h>
#include <root/component.h>
#include <cap_session/connection.h>
#include <log_session/log_session.h>
#include <nitpicker_session/connection.h>
#include <timer_session/connection.h>
#include <input/event.h>
#include <os/pixel_rgb565.h>

/*
 * Nitpicker's graphics backend
 */
#include <nitpicker_gfx/text_painter.h>
#include <nitpicker_gfx/box_painter.h>


enum { LOG_W = 80 };  /* number of visible characters per line */
enum { LOG_H = 25 };  /* number of lines of log window         */

typedef Text_painter::Font          Font;
typedef Genode::Surface_base::Point Point;
typedef Genode::Surface_base::Area  Area;
typedef Genode::Surface_base::Rect  Rect;
typedef Genode::Color               Color;


/*
 * Font initialization
 */
extern char _binary_mono_tff_start;
Font default_font(&_binary_mono_tff_start);


/**
 * Pixel-type-independent interface to graphics backend
 */
struct Canvas_base
{
	virtual void draw_string(Point, Font const &, Color, char const *) = 0;

	virtual void draw_box(Rect, Color) = 0;
};


/**
 * Pixel-type-specific graphics backend
 */
template <typename PT>
class Canvas : public Canvas_base
{
	private:

		Genode::Surface<PT> _surface;

	public:

		Canvas(PT *base, Area size) : _surface(base, size) { }

		void clip(Rect rect) { _surface.clip(rect); }

		void draw_string(Point p, Font const &font, Color color,
		                 char const *sstr)
		{
			Text_painter::paint(_surface, p, font, color, sstr);
		}

		void draw_box(Rect rect, Color color)
		{
			Box_painter::paint(_surface, rect, color);
		}
};


class Log_entry
{
	private:

		typedef Genode::Color Color;

		char  _label[64];
		char  _text[LOG_W];
		char  _attr[LOG_W];
		Color _color;
		int   _label_len;
		int   _text_len;
		int   _id;

	public:

		/**
		 * Constructors
		 *
		 * The default constructor is used to build an array of log entries.
		 */
		Log_entry() { }

		Log_entry(Genode::Color color, const char *label, const char *log_text, const char *log_attr, int id):
			_color(color), _id(id)
		{
			Genode::strncpy(_label, label,    sizeof(_label));
			Genode::strncpy(_text,  log_text, sizeof(_text));

			_label_len = Genode::strlen(_label);
			_text_len  = Genode::strlen(_text);

			/* replace line feed at the end of the text with a blank */
			if (_text_len > 0 && _text[_text_len - 1] == '\n')
				_text[_text_len - 1] = ' ';

			Genode::memcpy(_attr, log_attr, _text_len);
		}

		/**
		 * Draw entry
		 *
		 * An entry consists of a label and text. The argument 'new_section'
		 * marks a transition of output from one session to another. This
		 * information is used to separate sessions visually.
		 */
		void draw(Canvas_base &canvas, int y, int new_section = false)
		{
			Color label_fgcol = Color(Genode::min(255, _color.r + 200),
			                          Genode::min(255, _color.g + 200),
			                          Genode::min(255, _color.b + 200));
			Color label_bgcol = Color(_color.r, _color.g, _color.b);
			Color text_fgcol  = Color(180, 180, 180);
			Color text_bgcol  = Color(_color.r / 2, _color.g / 2, _color.b / 2);

			/* calculate label dimensions */
			int label_w = default_font.str_w(_label);
			int label_h = default_font.str_h(_label);

			if (new_section) {
				canvas.draw_box(Rect(Point(1, y), Area(label_w + 2, label_h - 1)), label_bgcol);
				canvas.draw_string(Point(1, y - 1), default_font, label_fgcol, _label);
				canvas.draw_box(Rect(Point(1, y + label_h - 1), Area(label_w + 2, 1)), Color(0, 0, 0));
				canvas.draw_box(Rect(Point(label_w + 2, y), Area(1, label_h - 1)), _color);
				canvas.draw_box(Rect(Point(label_w + 3, y), Area(1, label_h - 1)), Color(0, 0, 0));
				canvas.draw_box(Rect(Point(label_w + 4, y), Area(1000, label_h)), text_bgcol);
				canvas.draw_box(Rect(Point(label_w + 4, y), Area(1000, 1)), Color(0, 0, 0));
			} else
				canvas.draw_box(Rect(Point(1, y), Area(1000, label_h)), text_bgcol);

			/* draw log text */
			canvas.draw_string(Point(label_w + 6, y), default_font, text_fgcol, _text);
		}

		/**
		 * Accessors
		 */
		int label_len() { return _label_len; }
		int id()        { return _id; }
};


class Log_window
{
	private:

		Canvas_base &_canvas;
		Log_entry    _entries[LOG_H]; /* log entries                           */
		int          _dst_entry;      /* destination entry for next write      */
		int          _view_pos;       /* current view port on the entry array  */
		bool         _scroll;         /* scroll mode (when text hits bottom)   */
		char         _attr[LOG_W];    /* character attribute buffer            */
		bool         _dirty;          /* schedules the log window for a redraw */
		Genode::Lock _dirty_lock;

	public:

		/**
		 * Constructor
		 */
		Log_window(Canvas_base &canvas)
		: _canvas(canvas), _dst_entry(0), _view_pos(0), _dirty(true)
		{ }

		/**
		 * Write log entry
		 *
		 * \param color  base color for highlighting the session.
		 * \param sid    unique ID of the log session. This ID is used to
		 *               determine section transitions in the log output.
		 */
		void write(Genode::Color color, const char *label,
		           const char *log_text, int sid)
		{
			_entries[_dst_entry] = Log_entry(color, label, log_text, _attr, sid);

			if (_scroll)
				_view_pos++;

			/* cycle through log entries */
			_dst_entry = (_dst_entry + 1) % LOG_H;

			/* start scrolling when the dst entry wraps for the first time */
			if (_dst_entry == 0)
				_scroll = true;

			/* schedule log window for redraw */
			Genode::Lock::Guard lock_guard(_dirty_lock);
			_dirty |= 1;
		}

		/**
		 * Draw log window
		 *
		 * \retval true  drawing operations had been performed
		 */
		bool draw()
		{
			{
				Genode::Lock::Guard lock_guard(_dirty_lock);
				if (!_dirty) return false;
				_dirty = false;
			}

			int line_h = default_font.str_h(" ");
			int curr_session_id = -1;

			for (int i = 0, y = 0; i < LOG_H; i++, y += line_h) {
				Log_entry *le = &_entries[(i + _view_pos) % LOG_H];
				le->draw(_canvas, y, curr_session_id != le->id());
				curr_session_id = le->id();
			}

			return true;
		}
};


class Log_session_component : public Genode::Rpc_object<Genode::Log_session>
{
	public:

		enum { LABEL_LEN = 64 };

	private:

		Genode::Color _color;
		Log_window   *_log_window;
		char          _label[LABEL_LEN];
		int           _id;

		static int _bit(int v, int bit_num) { return (v >> bit_num) & 1; }

	public:

		/**
		 * Constructor
		 */
		Log_session_component(const char *label, Log_window *log_window)
		: _color(0, 0, 0), _log_window(log_window)
		{
			static int cnt;

			_id = cnt++;

			const int scale  = 32;
			const int offset = 64;

			/* compute session color */
			int r = (_bit(_id, 3) + 2*_bit(_id, 0))*scale + offset;
			int g = (_bit(_id, 4) + 2*_bit(_id, 1))*scale + offset;
			int b = (_bit(_id, 5) + 2*_bit(_id, 2))*scale + offset;

			_color = Genode::Color(r, g, b);

			Genode::strncpy(_label, label, sizeof(_label));
		}


		/***************************
		 ** Log session interface **
		 ***************************/

		Genode::size_t write(String const &log_text)
		{
			if (!log_text.valid_string()) {
				Genode::error("corrupted string");
				return 0;
			}

			_log_window->write(_color, _label, log_text.string(), _id);
			return Genode::strlen(log_text.string());
		}
};


class Log_root_component : public Genode::Root_component<Log_session_component>
{
	private:

		Log_window *_log_window;

	protected:

		Log_session_component *_create_session(const char *args)
		{
			Genode::log("create log session args: ", args);
			char label_buf[Log_session_component::LABEL_LEN];

			Genode::Arg label_arg = Genode::Arg_string::find_arg(args, "label");
			label_arg.string(label_buf, sizeof(label_buf), "");

			return new (md_alloc()) Log_session_component(label_buf, _log_window);
		}

	public:

		/**
		 * Constructor
		 */
		Log_root_component(Genode::Rpc_entrypoint *ep,
		                   Genode::Allocator      *md_alloc,
		                   Log_window             *log_window)
		:
			Genode::Root_component<Log_session_component>(ep, md_alloc),
			_log_window(log_window) { }
};


class Log_view
{
	private:

		Nitpicker::Session_client      &_nitpicker;
		Nitpicker::Point                _pos;
		Nitpicker::Area                 _size;
		Nitpicker::Session::View_handle _handle;

		typedef Nitpicker::Session::Command Command;

	public:

		Log_view(Nitpicker::Session_client &nitpicker, Nitpicker::Rect geometry)
		:
			_nitpicker(nitpicker),
			_pos(geometry.p1()),
			_size(geometry.area()),
			_handle(nitpicker.create_view())
		{
			move(_pos);
			top();
		}

		void top()
		{
			_nitpicker.enqueue<Command::To_front>(_handle);
			_nitpicker.execute();
		}

		void move(Nitpicker::Point pos)
		{
			_pos = pos;

			Nitpicker::Rect rect(_pos, _size);
			_nitpicker.enqueue<Command::Geometry>(_handle, rect);
			_nitpicker.execute();
		}

		Nitpicker::Point pos() const { return _pos; }
};


int main(int argc, char **argv)
{
	using namespace Genode;

	/* make sure that we connect to LOG before providing this service by ourself */
	log("--- nitlog ---");

	/* calculate size of log view in pixels */
	int log_win_w = default_font.str_w(" ") * LOG_W + 2;
	int log_win_h = default_font.str_h(" ") * LOG_H + 2;

	/* init sessions to the required external services */
	static Nitpicker::Connection nitpicker;
	static     Timer::Connection timer;

	nitpicker.buffer(Framebuffer::Mode(log_win_w, log_win_h,
	                 Framebuffer::Mode::RGB565), false);

	/* initialize entry point that serves the root interface */
	enum { STACK_SIZE = 4096*sizeof(long) };
	static Cap_connection cap;
	static Rpc_entrypoint ep(&cap, STACK_SIZE, "nitlog_ep");

	/*
	 * Use sliced heap to allocate each session component at a separate
	 * dataspace.
	 */
	static Sliced_heap sliced_heap(env()->ram_session(), env()->rm_session());

	/* create log window */
	void *addr = env()->rm_session()->attach(nitpicker.framebuffer()->dataspace());

	static Canvas<Pixel_rgb565> canvas((Pixel_rgb565 *)addr,
	                                          ::Area(log_win_w, log_win_h));
	static Log_window log_window(canvas);

	/*
	 * We clip a border of one pixel off the canvas. This way, the
	 * border remains unaffected by the drawing operations and
	 * acts as an outline for the log window.
	 */
	canvas.clip(::Rect(::Point(1, 1), ::Area(log_win_w - 2, log_win_h - 2)));

	/* create view for log window */
	Nitpicker::Rect log_view_geometry(Nitpicker::Point(20, 20),
	                                  Nitpicker::Area(log_win_w, log_win_h));
	Log_view log_view(nitpicker, log_view_geometry);

	/* create root interface for service */
	static Log_root_component log_root(&ep, &sliced_heap, &log_window);

	/* announce service at our parent */
	env()->parent()->announce(ep.manage(&log_root));

	/* handle input events */
	Input::Event *ev_buf = env()->rm_session()->attach(nitpicker.input()->dataspace());
	Nitpicker::Point old_mouse_pos;
	unsigned key_cnt = 0;
	while (1) {

		while (!nitpicker.input()->pending()) {
			if (log_window.draw())
				nitpicker.framebuffer()->refresh(0, 0, log_win_w, log_win_h);
			timer.msleep(20);
		}

		for (int i = 0, num_ev = nitpicker.input()->flush(); i < num_ev; i++) {

			Input::Event *ev = &ev_buf[i];

			if (ev->type() == Input::Event::PRESS)   key_cnt++;
			if (ev->type() == Input::Event::RELEASE) key_cnt--;

			Nitpicker::Point mouse_pos(ev->ax(), ev->ay());

			/* move view */
			if (ev->type() == Input::Event::MOTION && key_cnt > 0)
				log_view.move(log_view.pos() + mouse_pos - old_mouse_pos);

			/* find selected view and bring it to front */
			if (ev->type() == Input::Event::PRESS && key_cnt == 1)
				log_view.top();

			old_mouse_pos = mouse_pos;
		}
	}
	return 0;
}
