/*
 * \brief  Loadbar widget
 * \author Norman Feske
 * \date   2006-08-30
 */

/*
 * Copyright (C) 2006-2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _LOADBAR_H_
#define _LOADBAR_H_

#include "widgets.h"
#include "styles.h"
#include "fade_icon.h"

#include <base/printf.h>
#include <base/snprintf.h>


#define LOADBAR_RGBA _binary_loadbar_rgba_start
#define REDBAR_RGBA _binary_redbar_rgba_start
#define WHITEBAR_RGBA    _binary_whitebar_rgba_start
extern unsigned char LOADBAR_RGBA[];
extern unsigned char REDBAR_RGBA[];
extern unsigned char WHITEBAR_RGBA[];

class Loadbar_listener
{
	public:

		virtual ~Loadbar_listener() { }

		virtual void loadbar_changed(int mx) = 0;
};


class Loadbar_event_handler : public Event_handler
{
	private:

		Loadbar_listener *_listener;

	public:

		Loadbar_event_handler(Loadbar_listener *listener):
			_listener(listener) { }

		/**
		 * Event handler interface
		 */
		void handle(Event &ev)
		{
			static int key_cnt;

			if (ev.type == Event::PRESS)   key_cnt++;
			if (ev.type == Event::RELEASE) key_cnt--;

			if (ev.type == Event::PRESS || ev.type == Event::MOTION)
				if (_listener && key_cnt > 0)
					_listener->loadbar_changed(ev.mx);
		}
};


template <typename PT>
class Loadbar : public Parent_element
{
	private:

		enum {
			_LW = 16,
			_LH = 16,
		};

		bool _active;

		Fade_icon<PT, _LW, _LH> _cover;
		Fade_icon<PT, _LW, _LH> _bar;

		Loadbar_event_handler _ev_handler;

		int _value;
		int _max_value;

		const char *_txt;
		int _txt_w, _txt_h, _txt_len;
		Font *_font;

		void _update_bar_geometry(int w)
		{
			int max_w = w - _LW;
			int bar_w = (_value * max_w) / _max_value;
			bar_w += _LW;
			_bar.geometry(_bar.x(), _bar.y(), bar_w, _LH);
		}

	public:

		Loadbar(Loadbar_listener *listener = 0, Font *font = 0):
			_active(listener ? true : false),
			_ev_handler(listener),
			_value(0), _max_value(100),
			_txt(""), _txt_w(0), _txt_h(0), _txt_len(0),
			_font(font)
		{
			_min_h = _LH;
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
			int max_w = _w - _LW;
			return max(min((_max_value * xpos) / max_w, _max_value), 0);
		}

		int value() { return _value; }

		void value(int value)
		{
			_value = max(min(value, _max_value), 0);
			_update_bar_geometry(_w);
		}

		int  max_value() { return _max_value; }

		void max_value(int max_value)
		{
			_max_value = max_value;
			_update_bar_geometry(_w);
		}

		void txt(const char *txt)
		{
			if (!_font) return;
			_txt     = txt;
			_txt_w   = _font->str_w(_txt, strlen(_txt));
			_txt_h   = _font->str_h(_txt, strlen(_txt));
			_txt_len = strlen(_txt);
		}

		/**
		 * Element interface
		 */
		void format_fixed_width(int w)
		{
			_cover.geometry(0, 0, w, _LH);
			_update_bar_geometry(w);
			_min_w = w;
		}

		void draw(Canvas *c, int x, int y)
		{
			Parent_element::draw(c, x, y);

			if (!_font) return;

			int txt_x = x + _x + max((_w - _txt_w)/2, 8);
			int txt_y = y + _y + max((_h - _txt_h)/2, 0) - 1;

			/* shrink clipping area to text area (limit too long label) */
			int cx1 = c->clip_x1(), cy1 = c->clip_y1();
			int cx2 = c->clip_x2(), cy2 = c->clip_y2();
			int nx1 = max(cx1, _x + x);
			int ny1 = max(cy1, _y + y);
			int nx2 = min(cx2, nx1 + _w - 8);
			int ny2 = min(cy2, ny1 + _h);
			c->clip(nx1, ny1, nx2 - nx1 + 1, ny2 - ny1 + 1);

			c->draw_string(txt_x , txt_y+1, _font, Color(0,0,0,150), _txt, strlen(_txt));
			c->draw_string(txt_x , txt_y, _font, Color(255,255,255,230), _txt, strlen(_txt));

			/* reset clipping */
			c->clip(cx1, cy1, cx2 - cx1 + 1, cy2 - cy1 + 1);
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

		Kbyte_loadbar(Loadbar_listener *listener, Font *font = 0):
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
