/*
 * \brief   Scrollbar implementation
 * \date    2005-10-24
 * \author  Norman Feske <norman.feske@genode-labs.com>
 */

/*
 * Copyright (C) 2005-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <scout/tick.h>

#include "scrollbar.h"

using namespace Scout;

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

		/*
		 * Noncopyable
		 */
		Arrow_event_handler(Arrow_event_handler const &);
		Arrow_event_handler &operator = (Arrow_event_handler const &);

		/**
		 * Constants
		 */
		static const int _max_speed = 16*256;

		Scrollbar<PT>         *_sb;
		Fade_icon<PT, 32, 32> *_icon;
		unsigned char         *_rgba;
		int                    _direction;
		int                    _curr_speed = 0;
		int                    _dst_speed  = 0;
		int                    _view_pos   = 0;
		int                    _accel      = 1;

	public:

		/**
		 * Constructor
		 */
		Arrow_event_handler(Scrollbar<PT>         *sb,
		                    Fade_icon<PT, 32, 32> *icon,
		                    int                    direction,
		                    unsigned char         *rgba)
		:
			_sb(sb), _icon(icon), _rgba(rgba), _direction(direction)
		{ }

		/**
		 * Event handler interface
		 */
		void handle_event(Event const &ev) override
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

		/*
		 * Noncopyable
		 */
		Slider_event_handler(Slider_event_handler const &);
		Slider_event_handler &operator = (Slider_event_handler const &);

		Scrollbar<PT>         *_sb;
		Fade_icon<PT, 32, 32> *_icon;
		unsigned char         *_rgba;

	public:

		/**
		 * Constructor
		 */
		Slider_event_handler(Scrollbar<PT>         *sb,
		                     Fade_icon<PT, 32, 32> *icon,
		                     unsigned char         *rgba)
		:
			_sb(sb), _icon(icon), _rgba(rgba)
		{ }

		/**
		 * Event handler interface
		 */
		void handle_event(Event const &ev) override
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

				orig_my = curr_my = ev.mouse_position.y();
				orig_slider_pos = _sb->slider_pos();
			}

			if ((ev.type == Event::RELEASE) && (key_cnt == 0)) {

				/* release icon */
				_icon->rgba(_rgba);
				_icon->refresh();
			}

			if (key_cnt && (ev.mouse_position.y() != curr_my)) {
				curr_my = ev.mouse_position.y();
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

	_min_size = Area(sb_elem_w, sb_elem_h*3);

	/* define event handlers for scrollbar elements */
	_uparrow.event_handler(new Arrow_event_handler<PT>(this, &_uparrow, -1, UPARROW_RGBA));
	_dnarrow.event_handler(new Arrow_event_handler<PT>(this, &_dnarrow,  1, DNARROW_RGBA));
	_slider.event_handler(new Slider_event_handler<PT>(this, &_slider, SLIDER_RGBA));
}


template <typename PT>
int Scrollbar<PT>::slider_size()
{
	return max((unsigned)sb_elem_h, ((_size.h() - sb_elem_h*2)*_view_size)/_real_size);
}


template <typename PT>
int Scrollbar<PT>::slider_pos()
{
	int real_range   = _real_size - _view_size;
	int slider_range = _size.h() - sb_elem_h*2 - slider_size();
	int pos = real_range ? (slider_range*_view_pos)/real_range : 0;

	return pos + sb_elem_h;
}


template <typename PT>
void Scrollbar<PT>::slider_pos(int pos)
{
	int slider_bg_h = _size.h() - sb_elem_h*2;

	_view_pos = ((pos - sb_elem_h)*_real_size)/slider_bg_h;
	_view_pos = max(0, min(_view_pos,  _real_size - _view_size));

	_slider.geometry(Rect(Point(0, slider_pos()), Area(sb_elem_w, slider_size())));
}


template <typename PT>
void Scrollbar<PT>::view(int real_size, int view_size, int view_pos)
{
	_real_size = real_size;
	_view_size = min(view_size, real_size);
	_view_pos  = max(0, min(view_pos,  _real_size - _view_size));

	geometry(Rect(_position, _size));
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
void Scrollbar<PT>::geometry(Rect rect)
{
	Element::geometry(rect);

	int new_visibility = _visible();

	if (new_visibility) {
		_uparrow.geometry(Rect(Point(0, 0), Area(sb_elem_w, sb_elem_h)));
		_dnarrow.geometry(Rect(Point(0, rect.h() - sb_elem_h),
		                       Area(sb_elem_w, sb_elem_h)));
		_slider. geometry(Rect(Point(0, slider_pos()),
		                       Area(sb_elem_w, slider_size())));
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
Element *Scrollbar<PT>::find(Point position)
{
	if (_visibility)
		return Parent_element::find(position);

	return 0;
}


template class Scrollbar<Genode::Pixel_rgb565>;
