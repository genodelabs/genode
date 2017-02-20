/*
 * \brief   Browser window implementation
 * \date    2005-10-24
 * \author  Norman Feske <norman.feske@genode-labs.com>
 *
 * This class defines the layout and user policy of a browser window.
 */

/*
 * Copyright (C) 2005-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <scout_gfx/random.h>
#include <scout/misc_math.h>

#include "browser_window.h"

using namespace Scout;


/****************************
 ** External graphics data **
 ****************************/

#define IOR_MAP       _binary_ior_map_start
#define HOME_RGBA     _binary_home_rgba_start
#define COVER_RGBA    _binary_cover_rgba_start
#define INDEX_RGBA    _binary_index_rgba_start
#define ABOUT_RGBA    _binary_about_rgba_start
#define FORWARD_RGBA  _binary_forward_rgba_start
#define BACKWARD_RGBA _binary_backward_rgba_start
#define SIZER_RGBA    _binary_sizer_rgba_start
#define TITLEBAR_RGBA _binary_titlebar_rgba_start

extern short         IOR_MAP[];
extern unsigned char HOME_RGBA[];
extern unsigned char COVER_RGBA[];
extern unsigned char INDEX_RGBA[];
extern unsigned char ABOUT_RGBA[];
extern unsigned char FORWARD_RGBA[];
extern unsigned char BACKWARD_RGBA[];
extern unsigned char SIZER_RGBA[];
extern unsigned char TITLEBAR_RGBA[];

enum {
	ICON_HOME     = 0,
	ICON_BACKWARD = 1,
	ICON_FORWARD  = 2,
	ICON_INDEX    = 3,
	ICON_ABOUT    = 4,
	NUM_ICONS     = 5
};

/* icon graphics data */
static unsigned char *glow_icon_gfx[] = {
	HOME_RGBA,
	BACKWARD_RGBA,
	FORWARD_RGBA,
	INDEX_RGBA,
	ABOUT_RGBA,
};

/* color definitions for glowing effect of the icons */
static Color glow_icon_col[] = {
	Color(210, 210,   0),
	Color(  0,   0, 160),
	Color(  0,   0, 160),
	Color(  0, 160,   0),
	Color(160,   0,   0),
};


/***************
 ** Utilities **
 ***************/

/**
 * Transform rgba source image to image with native pixel type
 *
 * If we specify an empty buffer as alpha channel (all values zero), we simply
 * assign the source image data to the destination buffer. If there are valid
 * values in the destination alpha channel, we paint the source image on top
 * of the already present image data. This enables us to combine multiple
 * rgba buffers (layers) into one destination buffer.
 */
template <typename PT>
static void extract_rgba(const unsigned char *src, int w, int h,
                         PT *dst_pixel, unsigned char *dst_alpha)
{
	for (int i = 0; i < w*h; i++, src += 4) {

		int r = src[0];
		int g = src[1];
		int b = src[2];
		int a = src[3];

		if (dst_alpha[i]) {
			PT s(r, g, b);
			dst_pixel[i] = PT::mix(dst_pixel[i], s, a);
			dst_alpha[i] = max((int)dst_alpha[i], a);
		} else {
			dst_pixel[i].rgba(r, g, b);
			dst_alpha[i] = a;
		}
	}
}


/********************
 ** Event handlers **
 ********************/

class Iconbar_event_handler : public Scout::Event_handler
{
	private:

		Fader   *_fader;
		Browser *_browser;
		int      _icon_id;

	public:

		/**
		 * Constructor
		 */
		Iconbar_event_handler(Fader *fader, int icon_id, Browser *browser)
		{
			_fader   = fader;
			_browser = browser;
			_icon_id = icon_id;
		}

		/**
		 * Event handler interface
		 */
		void handle_event(Event const &ev) override
		{
			static int key_cnt;

			if (ev.type == Event::PRESS)   key_cnt++;
			if (ev.type == Event::RELEASE) key_cnt--;

			/* start movement with zero speed */
			if ((ev.type != Event::PRESS) || (key_cnt != 1)) return;

			/* no flashing by default */
			int flash = 0;

			switch (_icon_id) {

				case ICON_HOME:

					flash |= _browser->go_home();
					break;

				case ICON_BACKWARD:

					flash |= _browser->go_backward();
					break;

				case ICON_FORWARD:

					flash |= _browser->go_forward();
					break;

				case ICON_INDEX:

					flash |= _browser->go_toc();
					break;

				case ICON_ABOUT:

					flash |= _browser->go_about();
					break;
			}

			/* flash clicked icon */
			if (0 && flash) {

				/* flash fader to the max */
				_fader->step(4);
				_fader->curr(190);
			}
		}
};


template <typename PT>
class Browser_sizer_event_handler : public Scout::Sizer_event_handler
{
	private:

		Browser_window<PT> *_browser_win;
		Anchor              *_ca;         /* original visible element     */

		/**
		 * Event handler interface
		 */
		void start_drag()
		{
			Sizer_event_handler::start_drag();
			_ca = _browser_win->curr_anchor();
		}

		void do_drag()
		{
			Sizer_event_handler::do_drag();
			_browser_win->go_to(_ca, 0);
		}

	public:

		/**
		 * Constructor
		 */
		Browser_sizer_event_handler(Browser_window<PT> *browser_win):
			Sizer_event_handler(browser_win)
		{
			_browser_win = browser_win;
		}
};


/******************************
 ** Browser window interface **
 ******************************/

template <typename PT>
Browser_window<PT>::Browser_window(Document *initial_content,
                                   Graphics_backend &gfx_backend,
                                   Point position, Area size,
                                   Area max_size, Config const &config)
:
	Browser(_IH + _TH), Window(gfx_backend, position, size, max_size, true),
	_config(config),
	_gfx_backend(gfx_backend)
{
	/* init attributes */
	_ypos     = 0;
	_document = initial_content;

	/* init docview and history with initial document */
	_docview.texture(&_texture);
	_docview.voffset(doc_offset());
	_history.add(initial_content);

	/* init icons */
	memset(_icon_fg,       0, sizeof(_icon_fg));
	memset(_icon_fg_alpha, 0, sizeof(_icon_fg_alpha));
	for (int i = 0; i < _NUM_ICONS; i++) {

		/* convert rgba raw image to PT pixel format and alpha channel */
		extract_rgba(COVER_RGBA, _IW, _IH,
		             _icon_fg[i][0], _icon_fg_alpha[i][0]);

		/* assign back buffer, foreground and distmap to icon */
		_icon[i].backbuf(_icon_backbuf[0], 1);
		_icon[i].distmap(IOR_MAP, _IW*2, _IH*2);
		_icon[i].foreground(_icon_fg[i][0], _icon_fg_alpha[i][0]);
		_icon[i].event_handler(new Mover_event_handler(this));

		/* apply foreground graphics to icon */
		extract_rgba(glow_icon_gfx[i], _IW, _IH,
		             _icon_fg[i][0], _icon_fg_alpha[i][0]);

		/* init glow icon */
		Fade_icon<PT, _IW, _IH> *fadeicon = &_glow_icon[i];

		fadeicon->glow(glow_icon_gfx[i], glow_icon_col[i]);
		fadeicon->default_alpha(0);
		fadeicon->focus_alpha(100);
		fadeicon->alpha(0);
		fadeicon->event_handler(new Iconbar_event_handler(fadeicon, i, this));
	}

	/*
	 * All icons share the same distmap. Therefore we need to scratch
	 * only one distmap to affect all icons.
	 */
	_icon[0].scratch(_SCRATCH);

	/* create panel tile texture */
	/*
	 * NOTE: The panel height must be the same as the icon height.
	 */
	using Scout::random;
	for (int j = 0; j < _PANEL_H; j++)
		for (int i = 0; i < _PANEL_W; i++) {
			_panel_fg       [j][i] = _icon_fg       [ICON_INDEX][j][i&0x1];
			_panel_fg_alpha [j][i] = _icon_fg_alpha [ICON_INDEX][j][i&0x1] + random()%3;
		}

	/* init panel background */
	_panel.backbuf(&_panel_backbuf[0][0]);
	_panel.distmap(_panel_distmap[0], _PANEL_W*2, _PANEL_H*2);
	_panel.foreground(_panel_fg[0], _panel_fg_alpha[0]);
	_panel.scratch(_SCRATCH);
	_panel.event_handler(new Mover_event_handler(this));

	/* resize handle */
	_sizer.rgba(SIZER_RGBA);
	_sizer.event_handler(new Browser_sizer_event_handler<PT>(this));
	_sizer.alpha(100);

	/* titlebar */
	_titlebar.rgba(TITLEBAR_RGBA);
	_titlebar.text(_document->title);
	_titlebar.event_handler(new Mover_event_handler(this));

	_min_size = Scout::Area(_NUM_ICONS*_IW, _IH + 250);

	/* adopt widgets as child elements */
	append(&_docview);
	for (int i = 0; i <= ICON_INDEX; i++) {
		append(&_icon[i]);
		append(&_glow_icon[i]);
	}
	append(&_panel);
	append(&_icon[ICON_ABOUT]);
	append(&_glow_icon[ICON_ABOUT]);
	append(&_shadow);
	append(&_scrollbar);

	if (_attr & ATTR_SIZER)    append(&_sizer);
	if (_attr & ATTR_TITLEBAR) append(&_titlebar);

	_scrollbar.listener(this);

	_content(initial_content);
}


template <typename PT>
void Browser_window<PT>::ypos_sb(int ypos, int update_scrollbar)
{
	if (ypos < -(int)_docview.size().h() + (int)_size.h())
		ypos = -(int)_docview.size().h() + (int)_size.h();

	_ypos = ypos <= 0 ? ypos : 0;

	_docview.geometry(Rect(Point(_docview.position().x(), _ypos),
	                       Area(_docview.size().w(), _docview.size().h())));

	if (update_scrollbar)
		_scrollbar.view(_docview.size().h(), _size.h(), -_ypos);

	refresh();
}


/***********************
 ** Browser interface **
 ***********************/

template <typename PT>
Element *Browser_window<PT>::_content()
{
	return _docview.content();
}


template <typename PT>
void Browser_window<PT>::_content(Element *content)
{
	if (!content || (content == _docview.content())) return;

	content->fill_cache(_gfx_backend.front());
	_docview.content(content);
	format(_size);
	_ypos = 0;
}


template <typename PT>
void Browser_window<PT>::format(Area size)
{
	unsigned w = size.w();
	unsigned h = size.h();

	/* limit browser window size to valid values */
	w = max(w, min_size().w());
	h = max(h, min_size().h());
	w = min(w, max_size().w());
	h = min(h, max_size().h());

	/* determine old scrollbar visibility */
	int old_sb_visibility = (_docview.min_size().h() > _size.h());

	/* assign new size to browser window */
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
	if (_attr & ATTR_TITLEBAR) {
		_titlebar.geometry(Rect(Point(y, 0), Area(_size.w(), _TH)));
		y += _TH;
	}

	/* position icons */
	for (int i = 0; i <= ICON_INDEX; i++) {
		_icon[i].geometry(Rect(Point(i*_IW, y), Area(_IW, _IH)));
		_glow_icon[i].geometry(Rect(Point(i*_IW, y), Area(_IW, _IH)));
	}
	_icon[ICON_ABOUT].geometry(Rect(Point(_size.w() - _IW, y), Area(_IW, _IH)));
	_glow_icon[ICON_ABOUT].geometry(Rect(Point(_size.w() - _IW, y), Area(_IW, _IH)));

	/* the panel is the space between the left icon set and the right about icon */
	int panel_x = _icon[ICON_INDEX].position().x() + _IW;
	_panel.geometry(Rect(Point(panel_x, y),
	                     Area(_icon[ICON_ABOUT].position().x() - panel_x, _IH)));

	y += _IH;

	_scrollbar.geometry(Rect(Point(w - _scrollbar.min_size().w() - _SB_XPAD, y + _SB_YPAD),
	                         Area(_scrollbar.min_size().w(),
	                              h - y - _SB_YPAD*2 - (_attr & ATTR_SIZER ? 8 : 0))));
	_shadow.geometry(Rect(Point(0, y), Area(_size.w(), 10)));

	if (_attr & ATTR_SIZER)
		_sizer.geometry(Rect(Point(_size.w() - 32, _size.h() - 32), Area(32, 32)));

	Window::format(_size);
}


template <typename PT>
Anchor *Browser_window<PT>::curr_anchor() { return find_by_y(doc_offset()); }


/**********************************
 ** Scrollbar listener interface **
 **********************************/

template <typename PT>
void Browser_window<PT>::handle_scroll(int view_pos)
{
	/*
	 * The handle scroll notification comes from the scrollbar,
	 * which already adjusted itself to the new view port.
	 * Therefore, we do not need to re-adjust it another time
	 * and call ypos() with update_scrollbar set to zero.
	 */
	ypos_sb(-view_pos, 0);
}

template class Browser_window<Genode::Pixel_rgb565>;
