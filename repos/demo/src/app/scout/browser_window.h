/*
 * \brief   Browser window interface
 * \date    2005-10-24
 * \author  Norman Feske <norman.feske@genode-labs.com>
 */

/*
 * Copyright (C) 2005-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _BROWSER_WINDOW_H_
#define _BROWSER_WINDOW_H_

#include <scout/platform.h>
#include <scout/window.h>

#include "elements.h"
#include "widgets.h"
#include "sky_texture.h"
#include "refracted_icon.h"
#include "scrollbar.h"
#include "browser.h"
#include "titlebar.h"

namespace Scout { template <typename PT> class Browser_window; }


template <typename PT>
class Scout::Browser_window : public Scrollbar_listener,
                              public Browser,
                              public Window
{
	enum {
		ICON_HOME     = 0,
		ICON_BACKWARD = 1,
		ICON_FORWARD  = 2,
		ICON_INDEX    = 3,
		ICON_ABOUT    = 4
	};

	private:

		/**
		 * Constants
		 */
		enum {
			_NUM_ICONS = 5,     /* number of icons          */
			_IW        = 64,    /* browser icon width       */
			_IH        = 64,    /* browser icon height      */
			_TH        = 32,    /* height of title bar      */
			_PANEL_W   = 320,   /* panel tile width         */
			_PANEL_H   = _IH,   /* panel tile height        */
			_SB_XPAD   = 5,     /* hor. pad of scrollbar    */
			_SB_YPAD   = 10,    /* vert. pad of scrollbar   */
			_SCRATCH   = 7      /* scratching factor        */
		};

		/**
		 * General properties
		 */
		Config const &_config;

		int _attr = _config.browser_attr;   /* attribute mask */

		/**
		 * Remember graphics backend used as texture allocator
		 */
		Graphics_backend &_gfx_backend;

		/**
		 * Widgets
		 */
		Titlebar<PT>               _titlebar;
		Sky_texture<PT, 512, 512>  _texture { _config.background_detail };
		PT                         _icon_fg        [_NUM_ICONS][_IH][_IW];
		unsigned char              _icon_fg_alpha  [_NUM_ICONS][_IH][_IW];
		Refracted_icon<PT, short>  _icon           [_NUM_ICONS];
		PT                         _icon_backbuf   [_IH*2][_IW*2];
		PT                         _panel_fg       [_PANEL_H][_PANEL_W];
		unsigned char              _panel_fg_alpha [_PANEL_H][_PANEL_W];
		short                      _panel_distmap  [_PANEL_H*2][_PANEL_W*2];
		Refracted_icon<PT, short>  _panel;
		PT                         _panel_backbuf  [_PANEL_H*2][_PANEL_W*2];
		Horizontal_shadow<PT, 160> _shadow;
		Scrollbar<PT>              _scrollbar;
		Fade_icon<PT, _IW, _IH>    _glow_icon[_NUM_ICONS];
		Docview                    _docview;
		Fade_icon<PT, 32, 32>      _sizer;

	protected:

		/**
		 * Browser interface
		 */
		void _content(Element *content);
		Element *_content();

	public:

		/**
		 * Browser window attributes
		 */
		enum {
			ATTR_SIZER    = 0x1,  /* browser window has resize handle  */
			ATTR_TITLEBAR = 0x2,  /* browser window has titlebar       */
			ATTR_BORDER   = 0x4,  /* draw black outline around browser */
		};

		/**
		 * Constructor
		 */
		Browser_window(Document *content, Graphics_backend &gfx_backend,
		               Point position, Area size, Area max_size,
		               Config const &config);

		/**
		 * Return visible document offset
		 */
		inline int doc_offset() { return 10 + _IH + (_attr & ATTR_TITLEBAR ? _TH : 0); }

		/**
		 * Define vertical scroll offset of document
		 *
		 * \param update_scrollbar  if set to one, adjust scrollbar properties
		 *                          to the new view position.
		 */
		void ypos_sb(int ypos, int update_scrollbar = 1);

		/**
		 * Browser interface
		 */
		void     format(Area);
		void     ypos(int ypos) { ypos_sb(ypos, 1); }
		Anchor  *curr_anchor();
		Browser *browser() { return this; }

		/**
		 * Element interface
		 */
		void draw(Canvas_base &canvas, Point abs_position)
		{
			Parent_element::draw(canvas, abs_position);

			if (_attr & ATTR_BORDER) {
				Color color(0, 0, 0);
				canvas.draw_box(0, 0, _size.w(), 1, color);
				canvas.draw_box(0, _size.h() - 1, _size.w(), 1, color);
				canvas.draw_box(0, 1, 1, _size.h() - 2, color);
				canvas.draw_box(_size.w() - 1, 1, 1, _size.h() - 2, color);
			}
		};

		/**
		 * Scrollbar listener interface
		 */
		void handle_scroll(int view_pos);
};

#endif /* _BROWSER_WINDOW_H_ */
