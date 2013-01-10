/*
 * \brief   Scrollbar implementation
 * \date    2005-10-24
 * \author  Norman Feske <norman.feske@genode-labs.com>
 */

/*
 * Copyright (C) 2005-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#include "scrollbar.h"
#include "tick.h"

#define  SLIDER_RGBA _binary_slider_rgba_start
#define UPARROW_RGBA _binary_uparrow_rgba_start
#define DNARROW_RGBA _binary_downarrow_rgba_start

extern unsigned char SLIDER_RGBA[];
extern unsigned char UPARROW_RGBA[];
extern unsigned char DNARROW_RGBA[];


/********************
 ** Event handlers **
 ********************/

template <typename PT>
class Arrow_event_handler : public Event_handler, public Tick
{
	private:

		/**
		 * Constants
		 */
		static const int _max_speed = 16*256;

		Scrollbar<PT>         *_sb;
		Fade_icon<PT, 32, 32> *_icon;
		unsigned char         *_rgba;
		int                    _direction;
		int                    _curr_speed;
		int                    _dst_speed;
		int                    _view_pos;
		int                    _accel;

	public:

		/**
		 * Constructor
		 */
		Arrow_event_handler(Scrollbar<PT> *sb,
		                    Fade_icon<PT, 32, 32> *icon,
		                    int direction,
		                    unsigned char *rgba)
		{
			_sb        = sb;
			_icon      = icon;
			_direction = direction;
			_accel     = 1;
			_rgba      = rgba;
		}

		/**
		 * Event handler interface
		 */
		void handle(Event &ev)
		{
			static int key_cnt;

			if (ev.type == Event::PRESS)   key_cnt++;
			if (ev.type == Event::RELEASE) key_cnt--;

			/* start movement with zero speed */
			if ((ev.type == Event::PRESS) && (key_cnt == 1)) {

				/* press icon (slight vertical shift, darker shadow) */
				_icon->rgba(_rgba, 1, 3);
				_icon->refresh();

				_curr_speed = _direction*256;
				_dst_speed  = _direction*_max_speed;
				_accel      = 16;
				_view_pos   = _sb->view_pos() << 8;
				schedule(10);
			}

			if ((ev.type == Event::RELEASE) && (key_cnt == 0)) {

				/* release icon */
				_icon->rgba(_rgba);
				_icon->refresh();

				_accel     = 64;
				_dst_speed = 0;
			}
		}

		/**
		 * Tick interface
		 */
		int on_tick()
		{
			/* accelerate */
			if (_curr_speed < _dst_speed)
				_curr_speed = min(_curr_speed + _accel, _dst_speed);

			/* decelerate */
			if (_curr_speed > _dst_speed)
				_curr_speed = max(_curr_speed - _accel, _dst_speed);

			/* soft stopping on boundaries */
			while ((_curr_speed < 0) && (_view_pos > 0)
				&& (_curr_speed*_curr_speed > _view_pos*_accel*4))
					_curr_speed = min(0, _curr_speed + _accel*4);

			int max_pos;
			while ((_curr_speed > 0)
			 && ((max_pos = (_sb->real_size() - _sb->view_size())*256 - _view_pos) > 0)
			 && (_curr_speed*_curr_speed > max_pos*_accel*4))
				_curr_speed = max(0, _curr_speed - _accel*4);

			/* move view position with current speed */
			_view_pos = max(0, _view_pos + _curr_speed);

			/* set new view position */
			int old_view_pos = _sb->view_pos();
			_sb->view(_sb->real_size(), _sb->view_size(), _view_pos>>8);
			if (old_view_pos != _sb->view_pos())
				_sb->notify_listener();

			/* keep ticking as long as we are on speed */
			return (_curr_speed != 0);
		}
};


template <typename PT>
class Slider_event_handler : public Event_handler
{
	private:

		Scrollbar<PT>         *_sb;
		Fade_icon<PT, 32, 32> *_icon;
		unsigned char         *_rgba;

	public:

		/**
		 * Constructor
		 */
		Slider_event_handler(Scrollbar<PT> *sb,
		                    Fade_icon<PT, 32, 32> *icon,
		                    unsigned char *rgba)
		{
			_sb        = sb;
			_icon      = icon;
			_rgba      = rgba;
		}

		/**
		 * Event handler interface
		 */
		void handle(Event &ev)
		{
			static int key_cnt;
			static int curr_my, orig_my;
			static int orig_slider_pos;

			if (ev.type == Event::PRESS)   key_cnt++;
			if (ev.type == Event::RELEASE) key_cnt--;

			/* start movement with zero speed */
			if ((ev.type == Event::PRESS) && (key_cnt == 1)) {

				/* press icon (slight vertical shift, darker shadow) */
				_icon->rgba(_rgba, 1, 3);
				_icon->refresh();

				orig_my = curr_my = ev.my;
				orig_slider_pos = _sb->slider_pos();
			}

			if ((ev.type == Event::RELEASE) && (key_cnt == 0)) {

				/* release icon */
				_icon->rgba(_rgba);
				_icon->refresh();
			}

			if (key_cnt && (ev.my != curr_my)) {
				curr_my = ev.my;
				_sb->slider_pos(orig_slider_pos + curr_my - orig_my);
				_sb->notify_listener();
			}
		}
};


/*************************
 ** Scrollbar interface **
 *************************/

template <typename PT>
Scrollbar<PT>::Scrollbar()
{
	/* init scrollbar elements */
	_slider.rgba(SLIDER_RGBA);
	_uparrow.rgba(UPARROW_RGBA);
	_dnarrow.rgba(DNARROW_RGBA);

	_uparrow.alpha(0);
	_dnarrow.alpha(0);
	_slider .alpha(0);

	append(&_uparrow);
	append(&_dnarrow);
	append(&_slider);

	_min_w = sb_elem_w;
	_min_h = sb_elem_h*3;

	_real_size  = 100;
	_view_size  = 100;
	_view_pos   = 0;
	_listener   = 0;
	_visibility = 0;

	/* define event handlers for scrollbar elements */
	_uparrow.event_handler(new Arrow_event_handler<PT>(this, &_uparrow, -1, UPARROW_RGBA));
	_dnarrow.event_handler(new Arrow_event_handler<PT>(this, &_dnarrow,  1, DNARROW_RGBA));
	_slider.event_handler(new Slider_event_handler<PT>(this, &_slider, SLIDER_RGBA));
}


template <typename PT>
int Scrollbar<PT>::slider_size()
{
	return max(sb_elem_h, ((_h - sb_elem_h*2)*_view_size)/_real_size);
}


template <typename PT>
int Scrollbar<PT>::slider_pos()
{
	int real_range   = _real_size - _view_size;
	int slider_range = _h - sb_elem_h*2 - slider_size();
	int pos = real_range ? (slider_range*_view_pos)/real_range : 0;

	return pos + sb_elem_h;
}


template <typename PT>
void Scrollbar<PT>::slider_pos(int pos)
{
	int slider_bg_h = _h - sb_elem_h*2;

	_view_pos = ((pos - sb_elem_h)*_real_size)/slider_bg_h;
	_view_pos = max(0, min(_view_pos,  _real_size - _view_size));

	_slider.geometry(0, slider_pos(), sb_elem_w, slider_size());
}


template <typename PT>
void Scrollbar<PT>::view(int real_size, int view_size, int view_pos)
{
	_real_size = real_size;
	_view_size = min(view_size, real_size);
	_view_pos  = max(0, min(view_pos,  _real_size - _view_size));

	geometry(_x, _y, _w, _h);
}


template <typename PT>
void Scrollbar<PT>::notify_listener()
{
	if (_listener)
		_listener->handle_scroll(_view_pos);
}


/***********************
 ** Element interface **
 ***********************/

template <typename PT>
void Scrollbar<PT>::geometry(int x, int y, int w, int h)
{
	Element::geometry(x, y, w, h);

	int new_visibility = _visible();

	if (new_visibility) {
		_uparrow.geometry(0, 0, sb_elem_w, sb_elem_h);
		_dnarrow.geometry(0, h - sb_elem_h, sb_elem_w, sb_elem_h);
		_slider. geometry(0, slider_pos(), sb_elem_w, slider_size());
	}

	if (_visibility ^ new_visibility) {
		int alpha = new_visibility ? _uparrow.default_alpha() : 0;
		int speed = new_visibility ? 3 : 2;
		_uparrow.fade_to(alpha, speed);
		_dnarrow.fade_to(alpha, speed);
		_slider. fade_to(alpha, speed);
	}

	_visibility = new_visibility;
}


template <typename PT>
Element *Scrollbar<PT>::find(int x, int y)
{
	if (_visibility)
		return Parent_element::find(x, y);

	return 0;
}


#include "canvas_rgb565.h"
template class Scrollbar<Pixel_rgb565>;
