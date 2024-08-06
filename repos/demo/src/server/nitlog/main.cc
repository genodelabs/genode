/*
 * \brief  Nitpicker-based logging service
 * \author Norman Feske
 * \date   2006-09-18
 */

/*
 * Copyright (C) 2006-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <util/arg_string.h>
#include <base/component.h>
#include <base/heap.h>
#include <base/rpc_server.h>
#include <base/session_label.h>
#include <root/component.h>
#include <log_session/log_session.h>
#include <gui_session/connection.h>
#include <timer_session/connection.h>
#include <input/event.h>
#include <os/pixel_rgb888.h>

/*
 * Nitpicker's graphics backend
 */
#include <nitpicker_gfx/box_painter.h>
#include <nitpicker_gfx/tff_font.h>


enum { LOG_W = 80 };  /* number of visible characters per line */
enum { LOG_H = 25 };  /* number of lines of log window         */

using Font  = Text_painter::Font;
using Point = Genode::Surface_base::Point;
using Area  = Genode::Surface_base::Area;
using Rect  = Genode::Surface_base::Rect;
using Color = Genode::Color;


/*
 * Builtin font
 */
extern char _binary_mono_tff_start[];


namespace Nitlog {

	class Session_component;
	class Root;
	struct Main;

	using namespace Genode;
}




/**
 * Pixel-type-independent interface to graphics backend
 */
struct Canvas_base : Genode::Interface
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
		                 char const *sstr) override
		{
			Text_painter::paint(_surface, Text_painter::Position(p.x, p.y),
			                    font, color, sstr);
		}

		void draw_box(Rect rect, Color color) override
		{
			Box_painter::paint(_surface, rect, color);
		}
};


class Log_entry
{
	private:

		using Color  = Genode::Color;
		using size_t = Genode::size_t;

		char   _label[64];
		char   _text[LOG_W];
		char   _attr[LOG_W];
		Color  _color { };
		size_t _label_len = 0;
		size_t _text_len  = 0;
		int    _id        = 0;

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
			Genode::copy_cstring(_label, label,    sizeof(_label));
			Genode::copy_cstring(_text,  log_text, sizeof(_text));

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
		void draw(Canvas_base &canvas, Font const &font, int y, int new_section = false)
		{
			Color label_fgcol = Color::clamped_rgb(_color.r + 200,
			                                       _color.g + 200,
			                                       _color.b + 200);
			Color label_bgcol = _color;
			Color text_fgcol  = Color::rgb(180, 180, 180);
			Color text_bgcol  = Color::rgb(_color.r / 2, _color.g / 2, _color.b / 2);

			/* calculate label dimensions */
			int label_w = font.string_width(_label).decimal();
			int label_h = font.bounding_box().h;

			if (new_section) {
				canvas.draw_box(Rect(Point(1, y), Area(label_w + 2, label_h - 1)), label_bgcol);
				canvas.draw_string(Point(1, y - 1), font, label_fgcol, _label);
				canvas.draw_box(Rect(Point(1, y + label_h - 1), Area(label_w + 2, 1)), Color::black());
				canvas.draw_box(Rect(Point(label_w + 2, y), Area(1, label_h - 1)), _color);
				canvas.draw_box(Rect(Point(label_w + 3, y), Area(1, label_h - 1)), Color::black());
				canvas.draw_box(Rect(Point(label_w + 4, y), Area(1000, label_h)), text_bgcol);
				canvas.draw_box(Rect(Point(label_w + 4, y), Area(1000, 1)), Color::black());
			} else
				canvas.draw_box(Rect(Point(1, y), Area(1000, label_h)), text_bgcol);

			/* draw log text */
			canvas.draw_string(Point(label_w + 6, y), font, text_fgcol, _text);
		}

		/**
		 * Accessors
		 */
		size_t label_len() { return _label_len; }
		int    id()        { return _id; }
};


class Log_window
{
	private:

		Canvas_base  &_canvas;
		Font  const  &_font;
		Log_entry     _entries[LOG_H];    /* log entries                           */
		int           _dst_entry = 0;     /* destination entry for next write      */
		int           _view_pos  = 0;     /* current view port on the entry array  */
		bool          _scroll    = false; /* scroll mode (when text hits bottom)   */
		char          _attr[LOG_W];       /* character attribute buffer            */
		bool          _dirty     = true;  /* schedules the log window for a redraw */
		Genode::Mutex _dirty_mutex { };

	public:

		/**
		 * Constructor
		 */
		Log_window(Canvas_base &canvas, Font const &font)
		: _canvas(canvas), _font(font) { }

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
			Genode::Mutex::Guard guard(_dirty_mutex);
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
				Genode::Mutex::Guard guard(_dirty_mutex);
				if (!_dirty) return false;
				_dirty = false;
			}

			int line_h = _font.bounding_box().h;
			int curr_session_id = -1;

			for (int i = 0, y = 0; i < LOG_H; i++, y += line_h) {
				Log_entry *le = &_entries[(i + _view_pos) % LOG_H];
				le->draw(_canvas, _font, y, curr_session_id != le->id());
				curr_session_id = le->id();
			}

			return true;
		}
};


class Nitlog::Session_component : public Rpc_object<Log_session>
{
	private:

		Log_window &_log_window;

		Session_label const _label;

		int const _id;

		static int _bit(int v, int bit_num) { return (v >> bit_num) & 1; }

		/**
		 * Compute session color
		 */
		static Color _session_color(int id)
		{
			int const scale  = 32;
			int const offset = 64;

			int r = (_bit(id, 3) + 2*_bit(id, 0))*scale + offset;
			int g = (_bit(id, 4) + 2*_bit(id, 1))*scale + offset;
			int b = (_bit(id, 5) + 2*_bit(id, 2))*scale + offset;

			return Color::clamped_rgb(r, g, b);
		}

		Color const _color = _session_color(_id);

	public:

		/**
		 * Constructor
		 */
		Session_component(Session_label const &label,
		                  Log_window &log_window, int &cnt)
		:
			_log_window(log_window), _label(label), _id(cnt++)
		{ }


		/***************************
		 ** Log session interface **
		 ***************************/

		void write(String const &log_text) override
		{
			if (!log_text.valid_string()) {
				error("corrupted string");
				return;
			}

			_log_window.write(_color, _label.string(), log_text.string(), _id);
		}
};


class Nitlog::Root : public Root_component<Session_component>
{
	private:

		Log_window &_log_window;

		/* session counter, used as a key to generate session colors */
		int _session_cnt = 0;

	protected:

		Session_component *_create_session(const char *args) override
		{
			log("create log session args: ", args);

			return new (md_alloc())
				Session_component(label_from_args(args),
				                  _log_window, _session_cnt);
		}

	public:

		/**
		 * Constructor
		 */
		Root(Entrypoint &ep, Allocator &md_alloc, Log_window &log_window)
		:
			Root_component<Session_component>(ep, md_alloc),
			_log_window(log_window)
		{ }
};


class Log_view
{
	private:

		Gui::Connection          &_gui;
		Gui::Point                _pos;
		Gui::Area                 _size;
		Gui::Session::View_handle _handle;

		using Command     = Gui::Session::Command;
		using View_handle = Gui::Session::View_handle;

	public:

		Log_view(Gui::Connection &gui, Gui::Rect geometry)
		:
			_gui(gui),
			_pos(geometry.at),
			_size(geometry.area),
			_handle(gui.create_view())
		{
			move(_pos);
			top();
		}

		void top()
		{
			_gui.enqueue<Command::To_front>(_handle, View_handle());
			_gui.execute();
		}

		void move(Gui::Point pos)
		{
			_pos = pos;

			Gui::Rect rect(_pos, _size);
			_gui.enqueue<Command::Geometry>(_handle, rect);
			_gui.execute();
		}

		Gui::Point pos() const { return _pos; }
};


struct Nitlog::Main
{
	Env &_env;

	Tff_font::Static_glyph_buffer<4096> _glyph_buffer { };

	Tff_font _font { _binary_mono_tff_start, _glyph_buffer };

	/* calculate size of log view in pixels */
	unsigned const _win_w = _font.bounding_box().w * LOG_W + 2;
	unsigned const _win_h = _font.bounding_box().h * LOG_H + 2;

	/* init sessions to the required external services */
	Gui::Connection   _gui   { _env };
	Timer::Connection _timer { _env };

	void _init_gui_buffer()
	{
		_gui.buffer(Framebuffer::Mode { .area = { _win_w, _win_h } }, false);
	}

	bool const _gui_buffer_initialized = (_init_gui_buffer(), true);

	Sliced_heap _sliced_heap { _env.ram(), _env.rm() };

	/* create log window */
	Attached_dataspace _fb_ds { _env.rm(), _gui.framebuffer.dataspace() };

	Canvas<Pixel_rgb888> _canvas { _fb_ds.local_addr<Pixel_rgb888>(),
	                               ::Area(_win_w, _win_h) };

	Log_window _log_window { _canvas, _font };

	void _init_canvas()
	{
		/*
		 * We clip a border of one pixel off the canvas. This way, the
		 * border remains unaffected by the drawing operations and
		 * acts as an outline for the log window.
		 */
		_canvas.clip(::Rect(::Point(1, 1), ::Area(_win_w - 2, _win_h - 2)));
	}

	bool const _canvas_initialized = (_init_canvas(), true);

	/* create view for log window */
	Gui::Rect const _view_geometry { Gui::Point(20, 20),
	                                 Gui::Area(_win_w, _win_h) };
	Log_view _view { _gui, _view_geometry };

	/* create root interface for service */
	Root _root { _env.ep(), _sliced_heap, _log_window };

	Attached_dataspace _ev_ds { _env.rm(), _gui.input.dataspace() };

	Gui::Point const _initial_mouse_pos { -1, -1 };

	Gui::Point _old_mouse_pos = _initial_mouse_pos;

	unsigned _key_cnt = 0;

	Signal_handler<Main> _input_handler {
		_env.ep(), *this, &Main::_handle_input };

	void _handle_input()
	{
		Input::Event const *ev_buf = _ev_ds.local_addr<Input::Event const>();

		for (int i = 0, num_ev = _gui.input.flush(); i < num_ev; i++) {

			Input::Event const &ev = ev_buf[i];

			if (ev.press())   _key_cnt++;
			if (ev.release()) _key_cnt--;

			/* move view */
			ev.handle_absolute_motion([&] (int x, int y) {

				Gui::Point const mouse_pos(x, y);

				if (_key_cnt && _old_mouse_pos != _initial_mouse_pos)
					_view.move(_view.pos() + mouse_pos - _old_mouse_pos);

				_old_mouse_pos = mouse_pos;
			});

			/* find selected view and bring it to front */
			if (ev.press() && _key_cnt == 1)
				_view.top();

		}
	}

	Signal_handler<Main> _timer_handler {
		_env.ep(), *this, &Main::_handle_timer };

	void _handle_timer()
	{
		if (_log_window.draw())
			_gui.framebuffer.refresh(0, 0, _win_w, _win_h);
	}

	Main(Env &env) : _env(env)
	{
		/* announce service at our parent */
		_env.parent().announce(_env.ep().manage(_root));

		_timer.sigh(_timer_handler);
		_timer.trigger_periodic(20*1000);

		_gui.input.sigh(_input_handler);
	}
};


void Component::construct(Genode::Env &env)
{
	static Nitlog::Main main(env);
}
