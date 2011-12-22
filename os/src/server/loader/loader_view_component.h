/*
 * \brief  Instance of the view interface provided to the client
 * \author Christian Prochaska
 * \date   2010-09-02
 */

/*
 * Copyright (C) 2010-2011 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _LOADER_VIEW_COMPONENT_H_
#define _LOADER_VIEW_COMPONENT_H_

/* Genode includes */
#include <nitpicker_view/capability.h>
#include <nitpicker_view/client.h>
#include <nitpicker_view/nitpicker_view.h>
#include <base/rpc_server.h>
#include <base/env.h>

namespace Nitpicker {

	class Loader_view_component : public Genode::Rpc_object<View>
	{
		private:

			View *_view;       /* the wrapped view */
			int   _x, _y, _w, _h, _buf_x, _buf_y;

		public:

			/**
			 * Constructor
			 */
			Loader_view_component(View_capability view_cap)
			{
				_view = new (Genode::env()->heap()) Nitpicker::View_client(view_cap);
			}

			~Loader_view_component()
			{
				Genode::destroy(Genode::env()->heap(), _view);
			}


			/******************************
			 ** Nitpicker view interface **
			 ******************************/

			int viewport(int x, int y, int w, int h,
			             int buf_x, int buf_y, bool redraw)
			{
				_x = x;
				_y = y;
				_w = w;
				_h = h;
				_buf_x = buf_x;
				_buf_y = buf_y;
				return _view->viewport(x, y, w, h, buf_x, buf_y, redraw);
			}

			int stack(Nitpicker::View_capability neighbor_cap, bool behind, bool redraw)
			{
				return _view->stack(neighbor_cap, behind, redraw);
			}

			int title(Title const &title)
			{
				return _view->title(title.string());
			}

			void get_viewport(int *x, int *y, int *w, int *h, int *buf_x, int *buf_y)
			{
				if (x) *x = _x;
				if (y) *y = _y;
				if (w) *w = _w;
				if (h) *h = _h;
				if (buf_x) *buf_x = _buf_x;
				if (buf_y) *buf_y = _buf_y;
			}
	};
}

#endif /* _LOADER_VIEW_COMPONENT_H_ */
