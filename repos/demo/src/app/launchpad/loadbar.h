/*
 * \brief  Loadbar widget
 * \author Norman Feske
 * \date   2006-08-30
 */

/*
 * Copyright (C) 2006-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _LOADBAR_H_
#define _LOADBAR_H_

#include "widgets.h"
#include "styles.h"
#include "fade_icon.h"

#include <base/printf.h>
#include <base/snprintf.h>


#define LOADBAR_RGBA  _binary_loadbar_rgba_start
#define REDBAR_RGBA   _binary_redbar_rgba_start
#define WHITEBAR_RGBA _binary_whitebar_rgba_start

extern unsigned char LOADBAR_RGBA[];
extern unsigned char REDBAR_RGBA[];
extern unsigned char WHITEBAR_RGBA[];

struct Loadbar_listener
{
	virtual ~Loadbar_listener() { }

	virtual void loadbar_changed(int mx) = 0;
};


class Loadbar_event_handler : public Scout::Event_handler
{
	private:

		/*
		 * Noncopyable
		 */
		Loadbar_event_handler(Loadbar_event_handler const &);
		Loadbar_event_handler &operator = (Loadbar_event_handler const &);

		Loadbar_listener *_listener;

	public:

		Loadbar_event_handler(Loadbar_listener *listener):
			_listener(listener) { }

		/**
		 * Event handler interface
		 */
		void handle_event(Scout::Event const &ev) override
		{
			static int key_cnt;
			using Scout::Event;

			if (ev.type == Event::PRESS)   key_cnt++;
			if (ev.type == Event::RELEASE) key_cnt--;

			if (ev.type == Event::PRESS || ev.type == Event::MOTION)
				if (_listener && key_cnt > 0)
					_listener->loadbar_changed(ev.mouse_position.x());
		}
};


template <typename PT>
class Loadbar : public Scout::Parent_element
{
	private:

		/*
		 * Noncopyable
		 */
		Loadbar(Loadbar const &);
		Loadbar &operator = (Loadbar const &);

		enum {
			_LW = 16,
			_LH = 16,
		};

		bool _active;

		Scout::Fade_icon<PT, _LW, _LH> _cover { };
		Scout::Fade_icon<PT, _LW, _LH> _bar   { };

		Loadbar_event_handler _ev_handler;

		int _value;
		int _max_value;

		const char *_txt;
		int _txt_w, _txt_h, _txt_len;
		Scout::Font *_font;

		void _update_bar_geometry(int w)
		{
			using namespace Scout;

			int max_w = w - _LW;
			int bar_w = (_value * max_w) / _max_value;
			bar_w += _LW;
			_bar.geometry(Rect(Point(_bar.position().x(), _bar.position().y()),
			                   Area(bar_w, _LH)));
		}

	public:

		Loadbar(Loadbar_listener *listener = 0, Scout::Font *font = 0):
			_active(listener ? true : false),
			_ev_handler(listener),
			_value(0), _max_value(100),
			_txt(""), _txt_w(0), _txt_h(0), _txt_len(0),
			_font(font)
		{
			using namespace Scout;

			_min_size = Area(_min_size.w(), _LH);
			_cover.rgba(LOADBAR_RGBA);
			_cover.alpha(100);
			_cover.focus_alpha(150);

			_bar.rgba(_active ? REDBAR_RGBA : LOADBAR_RGBA);
			_bar.alpha(_active ? 150 : 255);
			_bar.default_alpha(150);

			if (_active)
				event_handler(&_ev_handler);

			append(&_cover);
			append(&_bar);
		}

		int value_by_xpos(int xpos)
		{
			xpos -= _LW/2;
			int max_w = _size.w() - _LW;
			return Scout::max(Scout::min((_max_value * xpos) / max_w, _max_value), 0);
		}

		int value() { return _value; }

		void value(int value)
		{
			_value = Scout::max(Scout::min(value, _max_value), 0);
			_update_bar_geometry(_size.w());
		}

		int  max_value() { return _max_value; }

		void max_value(int max_value)
		{
			_max_value = max_value;
			_update_bar_geometry(_size.w());
		}

		void txt(const char *txt)
		{
			if (!_font) return;
			_txt     = txt;
			_txt_w   = _font->string_width(_txt, Scout::strlen(_txt)).decimal();
			_txt_h   = _font->bounding_box().h();
			_txt_len = Scout::strlen(_txt);
		}

		/**
		 * Element interface
		 */
		void format_fixed_width(int w)
		{
			using namespace Scout;
			_cover.geometry(Rect(Point(0, 0), Area(w, _LH)));
			_update_bar_geometry(w);
			_min_size = Scout::Area(w, _min_size.h());
		}

		void draw(Scout::Canvas_base &canvas, Scout::Point abs_position)
		{
			Parent_element::draw(canvas, abs_position);

			if (!_font) return;

			using namespace Scout;

			int txt_x = abs_position.x() + _position.x() + max((_size.w() - _txt_w)/2, 8UL);
			int txt_y = abs_position.y() + _position.y() + max((_size.h() - _txt_h)/2, 0UL) - 1;

			/* shrink clipping area to text area (limit too long label) */
			int cx1 = canvas.clip().x1(), cy1 = canvas.clip().y1();
			int cx2 = canvas.clip().x2(), cy2 = canvas.clip().y2();
			int nx1 = max(cx1, _position.x() + abs_position.x());
			int ny1 = max(cy1, _position.y() + abs_position.y());
			int nx2 = min(cx2, nx1 + (int)_size.w() - 8);
			int ny2 = min(cy2, ny1 + (int)_size.h());
			canvas.clip(Rect(Point(nx1, ny1), Area(nx2 - nx1 + 1, ny2 - ny1 + 1)));

			canvas.draw_string(txt_x , txt_y+1, _font, Color(0,0,0,150), _txt, strlen(_txt));
			canvas.draw_string(txt_x , txt_y, _font, Color(255,255,255,230), _txt, strlen(_txt));

			/* reset clipping */
			canvas.clip(Rect(Point(cx1, cy1), Area(cx2 - cx1 + 1, cy2 - cy1 + 1)));
		}

		void mfocus(int flag)
		{
			if (!_active) return;

			_bar.mfocus(flag);
			_cover.mfocus(flag);
		}
};


template <typename PT>
class Kbyte_loadbar : public Loadbar<PT>
{
	private:

		char _label[32];

		void _print_kbytes(int kbytes, char *dst, int dst_len)
		{
			if (kbytes >= 10*1024)
				Genode::snprintf(dst, dst_len, "%d MByte", kbytes / 1024);
			else
				Genode::snprintf(dst, dst_len, "%d KByte", kbytes);
		}

		void _update_label()
		{
			char value_buf[16];
			char max_buf[16];

			_print_kbytes(Loadbar<PT>::value(), value_buf, sizeof(value_buf));
			_print_kbytes(Loadbar<PT>::max_value(), max_buf, sizeof(max_buf));

			Genode::snprintf(_label, sizeof(_label), "%s / %s", value_buf, max_buf);

			Loadbar<PT>::txt(_label);
		}

	public:

		Kbyte_loadbar(Loadbar_listener *listener, Scout::Font *font = 0):
			Loadbar<PT>(listener, font)
		{
			_label[0] = 0;
			_update_label();
		}

		void value(int val)
		{
			Loadbar<PT>::value(val);
			_update_label();
		}

		void max_value(int max_value)
		{
			Loadbar<PT>::max_value(max_value);
			_update_label();
		}
};

#endif
