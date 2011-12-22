/*
 * \brief  Instance of the view interface used by the child
 * \author Christian Prochaska
 * \date   2009-10-21
 */

/*
 * Copyright (C) 2009-2011 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _NITPICKER_VIEW_COMPONENT_H_
#define _NITPICKER_VIEW_COMPONENT_H_

/* Genode includes */
#include <nitpicker_view/capability.h>
#include <nitpicker_view/client.h>
#include <base/rpc_server.h>
#include <base/env.h>
#include <os/timed_semaphore.h>

namespace Nitpicker {

	class View_component : public Genode::Rpc_object<View>
	{
		private:

			View_client _view;   /* the wrapped view */

			Genode::Timed_semaphore  *_ready_sem;

			int _x, _y, _w, _h, _buf_x, _buf_y;

			bool _viewport_initialized;

		public:

			/**
			 * Constructor
			 */
			View_component(View_capability view_cap, Genode::Timed_semaphore *ready_sem)
			: _view(view_cap), _ready_sem(ready_sem), _viewport_initialized(false)
			{ }


			/******************************
			 ** Nitpicker view interface **
			 ******************************/

			int viewport(int x, int y, int w, int h,
			             int buf_x, int buf_y, bool redraw)
			{
				_x = x; _y = y;
				_w = w; _h = h;
				_buf_x = buf_x; _buf_y = buf_y;

				if (_viewport_initialized)
					return 0;

				_viewport_initialized = true;

				/* hide the view and let the application set the viewport */
				int result = _view.viewport(0, 0, 0, 0, 0, 0, true);

				/* viewport data is available -> loader can continue */
				_ready_sem->up();

				return result;
			}

			int stack(View_capability neighbor_cap, bool behind, bool redraw)
			{
				/*
				 * Only one child view is supported, so the stack request can
				 * be ignored.
				 */
				return 0;
			}

			int title(Title const &title)
			{
				return _view.title(title.string());
			}

			void get_viewport(int *x, int *y, int *w, int *h, int *buf_x, int *buf_y)
			{
				if (x)     *x     = _x;
				if (y)     *y     = _y;
				if (w)     *w     = _w;
				if (h)     *h     = _h;
				if (buf_x) *buf_x = _buf_x;
				if (buf_y) *buf_y = _buf_y;
			}
	};
}

#endif /* _NITPICKER_VIEW_COMPONENT_H_ */
