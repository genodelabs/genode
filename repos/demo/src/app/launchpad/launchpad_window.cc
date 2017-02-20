/*
 * \brief   Launchpad window implementation
 * \date    2006-08-30
 * \author  Norman Feske
 */

/*
 * Copyright (C) 2006-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <scout/misc_math.h>

#include "launchpad_window.h"
#include "styles.h"

using namespace Scout;


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
Launchpad_window<PT>::Launchpad_window(Genode::Env &env,
                                       Graphics_backend &gfx_backend,
                                       Point position, Area size, Area max_size,
                                       unsigned long initial_quota)
:
	Launchpad(env, initial_quota),
	Window(gfx_backend, position, size, max_size, false),
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

	_min_size = Scout::Area(200, 200);

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
	if (ypos < -(int)(_docview.size().h() + _size.h()))
		ypos = -_docview.size().h() + _size.h();

	_ypos = ypos <= 0 ? ypos : 0;

	_docview.geometry(Rect(Point(_docview.position().x(), _ypos), _docview.size()));

	if (update_scrollbar)
		_scrollbar.view(_docview.size().h(), _size.h(), -_ypos);

	refresh();
}


/*************************
 ** Launchpad interface **
 *************************/

template <typename PT>
void Launchpad_window<PT>::format(Scout::Area size)
{
	/* limit window size to valid values */
	unsigned w = size.w();
	unsigned h = size.h();

	w = max(w, min_size().w());
	h = max(h, min_size().h());
	w = min(w, max_size().w());
	h = min(h, max_size().h());

	/* determine old scrollbar visibility */
	int old_sb_visibility = (_docview.min_size().h() > _size.h());

	/* assign new size to window */
	_size = Scout::Area(w, h);

	/* format document */
	_docview.format_fixed_width(_size.w());

	/* format titlebar */
	_titlebar.format_fixed_width(_size.w());

	/* determine new scrollbar visibility */
	int new_sb_visibility = (_docview.min_size().h() > _size.h());

	/* reformat docview on change of scrollbar visibility */
	if (old_sb_visibility ^ new_sb_visibility) {
		_docview.right_pad(new_sb_visibility ? _scrollbar.min_size().w() : 0);
		_docview.format_fixed_width(_size.w());
	}

	/* position docview */
	_docview.geometry(Rect(Point(0, _ypos),
	                       Area(_docview.min_size().w(),
	                            max(_docview.min_size().h(), _size.h()))));

	/* start at top */
	int y = 0;

	/* position titlebar */
	_titlebar.geometry(Rect(Point(y, 0), Area(_size.w(), _TH)));
	y += _TH;

	_scrollbar.geometry(Rect(Point(w - _scrollbar.min_size().w() - _SB_XPAD, y + _SB_YPAD),
	                         Area(_scrollbar.min_size().w(), h - y - _SB_YPAD*2 - 8)));
	
	
	_sizer.geometry(Rect(Point(_size.w() - 32, _size.h() - 32), Area(32, 32)));

	Window::format(_size);
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

template class Launchpad_window<Genode::Pixel_rgb565>;
