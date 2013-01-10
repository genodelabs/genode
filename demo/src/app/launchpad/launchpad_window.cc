/*
 * \brief   Launchpad window implementation
 * \date    2006-08-30
 * \author  Norman Feske
 */

/*
 * Copyright (C) 2006-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#include "miscmath.h"
#include "launchpad_window.h"
#include "styles.h"

/****************************
 ** External graphics data **
 ****************************/

#define SIZER_RGBA    _binary_sizer_rgba_start
#define TITLEBAR_RGBA _binary_titlebar_rgba_start

extern unsigned char SIZER_RGBA[];
extern unsigned char TITLEBAR_RGBA[];


/********************************
 ** Launchpad window interface **
 ********************************/

template <typename PT>
Launchpad_window<PT>::Launchpad_window(Platform *pf,
                                       Redraw_manager *redraw,
                                       int max_w, int max_h,
                                       unsigned long initial_quota)
:
	Launchpad(initial_quota),
	Window(pf, redraw, max_w, max_h),
	_docview(0),
	_spacer(1, _TH),
	_info_section("Status", &subsection_font),
	_launch_section("Launcher", &subsection_font),
	_kiddy_section("Children", &subsection_font),
	_status_entry("Quota")
{
	/* resize handle */
	_sizer.rgba(SIZER_RGBA);
	_sizer.event_handler(new Sizer_event_handler(this));
	_sizer.alpha(100);

	/* titlebar */
	_titlebar.rgba(TITLEBAR_RGBA);
	_titlebar.text("Launchpad");
	_titlebar.event_handler(new Mover_event_handler(this));

	_min_w = 200;
	_min_h = 200;

	_status_entry.max_value(initial_quota / 1024);

	/* adopt widgets as child elements */
	_info_section.append(&_status_entry);
	_document.append(&_spacer);
	_document.append(&_info_section);
	_document.append(&_launch_section);
	_document.append(&_kiddy_section);

	append(&_docview);
	append(&_titlebar);
	append(&_scrollbar);
	append(&_sizer);

	_scrollbar.listener(this);
	_docview.texture(&_texture);
	_docview.content(&_document);
}


template <typename PT>
void Launchpad_window<PT>::ypos_sb(int ypos, int update_scrollbar)
{
	if (ypos < -_docview.h() + _h)
		ypos = -_docview.h() + _h;

	_ypos = ypos <= 0 ? ypos : 0;

	_docview.geometry(_docview.x(), _ypos, _docview.w(), _docview.h());

	if (update_scrollbar)
		_scrollbar.view(_docview.h(), _h, -_ypos);

	refresh();
}


/*************************
 ** Launchpad interface **
 *************************/

template <typename PT>
void Launchpad_window<PT>::format(int w, int h)
{
	/* limit window size to valid values */
	w = (w < _min_w)  ? _min_w  : w;
	h = (h < _min_h)  ? _min_h  : h;
	w = (w > max_w()) ? max_w() : w;
	h = (h > max_h()) ? max_h() : h;

	/* determine old scrollbar visibility */
	int old_sb_visibility = (_docview.min_h() > _h);

	/* assign new size to window */
	_w = w;
	_h = h;

	/* format document */
	_docview.format_fixed_width(_w);

	/* format titlebar */
	_titlebar.format_fixed_width(_w);

	/* determine new scrollbar visibility */
	int new_sb_visibility = (_docview.min_h() > _h);

	/* reformat docview on change of scrollbar visibility */
	if (old_sb_visibility ^ new_sb_visibility) {
		_docview.right_pad(new_sb_visibility ? _scrollbar.min_w() : 0);
		_docview.format_fixed_width(_w);
	}

	/* position docview */
	_docview.geometry(0, _ypos, _docview.min_w(), max(_docview.min_h(), _h));

	/* start at top */
	int y = 0;

	/* position titlebar */
	_titlebar.geometry(y, 0, _w, _TH);
	y += _TH;

	_scrollbar.geometry(w - _scrollbar.min_w() - _SB_XPAD, y + _SB_YPAD,
	                        _scrollbar.min_w(), h - y - _SB_YPAD*2 - 8);
	
	
	_sizer.geometry(_w - 32, _h - 32, 32, 32);

	pf()->view_geometry(pf()->vx(), pf()->vy(), _w, _h);
	redraw()->size(_w, _h);
	ypos(_ypos);
	refresh();
}


/**********************************
 ** Scrollbar listener interface **
 **********************************/

template <typename PT>
void Launchpad_window<PT>::handle_scroll(int view_pos)
{
	/*
	 * The handle scroll notification comes from the scrollbar,
	 * which already adjusted itself to the new view port.
	 * Therefore, we do not need to re-adjust it another time
	 * and call ypos() with update_scrollbar set to zero.
	 */
	ypos_sb(-view_pos, 0);
}

#include "canvas_rgb565.h"
template class Launchpad_window<Pixel_rgb565>;
