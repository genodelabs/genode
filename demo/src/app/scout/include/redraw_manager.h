/*
 * \brief   Simplistic redraw manager featuring redraw merging
 * \date    2005-10-24
 * \author  Norman Feske <norman.feske@genode-labs.com>
 */

/*
 * Copyright (C) 2005-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _REDRAW_MANAGER_H_
#define _REDRAW_MANAGER_H_

#include "elements.h"

class Redraw_manager
{
	private:

		int            _x1, _y1;       /* upper  left pixel of dirty area   */
		int            _x2, _y2;       /* lower right pixel of dirty area   */
		int            _cnt;           /* nb of requests since last process */
		Element       *_root;          /* root element for drawing          */
		Canvas        *_canvas;        /* graphics backend                  */
		Screen_update *_scr_update;    /* flushing pixels in backend        */
		int            _w, _h;         /* current size of output window     */
		bool           _scout_quirk;   /* enable redraw quirk for scout     */

	public:

		/**
		 * Constructor
		 */
		Redraw_manager(Canvas *canvas, Screen_update *scr_update, int w, int h,
		               bool scout_quirk = false)
		:
			_cnt(0), _root(0), _canvas(canvas), _scr_update(scr_update), _w(w), _h(h),
			_scout_quirk(scout_quirk)
		{ }

		/**
		 * Accessor functions
		 */
		inline Canvas *canvas() { return _canvas; }

		/**
		 * Define root element for issueing drawing operations
		 */
		inline void root(Element *root) { _root = root; }

		/**
		 * Collect redraw requests
		 */
		void request(int x, int y, int w, int h)
		{
			/*
			 * Scout redraw quirk
			 *
			 * Quick fix to avoid artifacts at the icon bar.
			 * The icon bar must always be drawn completely
			 * because of the interaction of the different
			 * layers.
			 */
			if (_scout_quirk && y < 64 + 32) {
				h = max(h + y, 64 + 32);
				w = _w;
				x = 0;
				y = 0;
			}

			/* first request since last process operation */
			if (_cnt == 0) {
				_x1 = x; _x2 = x + w - 1;
				_y1 = y; _y2 = y + h - 1;

			/* merge subsequencing requests */
			} else {
				_x1 = min(_x1, x); _x2 = max(_x2, x + w - 1);
				_y1 = min(_y1, y); _y2 = max(_y2, y + h - 1);
			}
			_cnt++;
		}

		/**
		 * Define size of visible redraw window
		 */
		void size(int w, int h)
		{
			if (w > _canvas->w())
				w = _canvas->w();

			if (h > _canvas->h())
				h = _canvas->h();

			_w = w; _h = h;
		}

		/**
		 * Process redrawing operations
		 */
		void process()
		{
			if (_cnt == 0 || !_canvas || !_root) return;

			/* get actual drawing area (clipped against canvas dimensions) */
			int x1 = max(0, _x1);
			int y1 = max(0, _y1);
			int x2 = min(_w - 1, _x2);
			int y2 = min(_h - 1, _y2);

			if (x1 > x2 || y1 > y2) return;

			_canvas->clip(x1, y1, x2 - x1 + 1, y2 - y1 + 1);

			/* draw browser window into back buffer */
			_root->try_draw(_canvas, 0, 0);

			/*
			 * If we draw the whole area, we can flip the front
			 * and back buffers instead of copying pixels from the
			 * back to the front buffer.
			 */

			/* detemine if the whole area must be drawn */
			if (x1 == 0 && x2 == _root->w() - 1
			 && y1 == 0 && y2 == _root->h() - 1) {

				/* flip back end front buffers */
				_scr_update->flip_buf_scr();

				/* apply future drawing operations on new back buffer */
				_canvas->addr(_scr_update->buf_adr());

			} else
				_scr_update->copy_buf_to_scr(x1, y1, x2 - x1 + 1, y2 - y1 + 1);

			/* give notification about changed canvas area */
			if (_scr_update)
				_scr_update->scr_update(x1, y1, x2 - x1 + 1, y2 - y1 + 1);

			/* reset request state */
			_cnt = 0;
		}
};


#endif /* _REDRAW_MANAGER_H_ */
