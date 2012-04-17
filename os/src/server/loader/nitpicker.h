/*
 * \brief  Virtualized nitpicker session interface exposed to the subsystem
 * \author Christian Prochaska
 * \author Norman Feske
 * \date   2009-10-05
 */

/*
 * Copyright (C) 2009-2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _NITPICKER_H_
#define _NITPICKER_H_

/* Genode includes */
#include <util/arg_string.h>
#include <util/misc_math.h>
#include <base/signal.h>
#include <nitpicker_session/connection.h>
#include <nitpicker_session/nitpicker_session.h>
#include <nitpicker_view/client.h>

/* local includes */
#include <input.h>

namespace Nitpicker {

	using namespace Genode;

	/**
	 * View interface provided to the loader client
	 */
	class Loader_view_component : public Rpc_object<View>
	{
		private:

			View_client _view;  /* wrapped view */

			int _x, _y, _buf_x, _buf_y;

		public:

			/**
			 * Constructor
			 */
			Loader_view_component(View_capability view_cap)
			:
				_view(view_cap), _x(0), _y(0), _buf_x(0), _buf_y(0)
			{ }

			int x()     const { return _x; }
			int y()     const { return _y; }
			int buf_x() const { return _buf_x; }
			int buf_y() const { return _buf_y; }


			/******************************
			 ** Nitpicker view interface **
			 ******************************/

			int viewport(int x, int y, int w, int h,
			             int buf_x, int buf_y, bool redraw)
			{
				_x = x, _y = y, _buf_x = buf_x, _buf_y = buf_y;

				return _view.viewport(x, y, w, h, buf_x, buf_y, redraw);
			}

			int stack(View_capability neighbor_cap, bool behind, bool redraw)
			{
				return _view.stack(neighbor_cap, behind, redraw);
			}

			int title(Title const &title)
			{
				return _view.title(title.string());
			}
	};


	/**
	 * View interface exposed to the subsystem
	 */
	class View_component : public Rpc_object<View>
	{
		private:

			View_client               _view;  /* wrapped view */
			Signal_context_capability _sigh;
			bool                      _viewport_initialized;
			int                       _x, _y, _w, _h, _buf_x, _buf_y;

		public:

			/**
			 * Constructor
			 */
			View_component(View_capability           view_cap,
			               Signal_context_capability sigh)
			:
				_view(view_cap), _sigh(sigh), _viewport_initialized(false),
				_x(0), _y(0), _w(0), _h(0), _buf_x(0), _buf_y(0)
			{ }

			int x()     const { return _x; }
			int y()     const { return _y; }
			int w()     const { return _w; }
			int h()     const { return _h; }
			int buf_x() const { return _buf_x; }
			int buf_y() const { return _buf_y; }


			/******************************
			 ** Nitpicker view interface **
			 ******************************/

			int viewport(int x, int y, int w, int h,
			             int buf_x, int buf_y, bool redraw)
			{
				_x = x; _y = y; _w = w; _h = h;
				_buf_x = buf_x; _buf_y = buf_y;

				if (_viewport_initialized)
					return 0;

				_viewport_initialized = true;

				/* hide the view and let the application set the viewport */
				int result = _view.viewport(0, 0, 0, 0, 0, 0, true);

				/* signal readyness of the view */
				if (_sigh.valid())
					Signal_transmitter(_sigh).submit();

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
	};


	class Session_component : public Rpc_object<Session>,
	                          public Input::Transformer
	{
		private:

			Rpc_entrypoint           &_ep;

			int                       _fb_width, _fb_height;

			Nitpicker::Connection     _nitpicker;
			View_capability           _nitpicker_view;

			View_component            _proxy_view;
			View_capability           _proxy_view_cap;

			Loader_view_component     _loader_view;
			View_capability           _loader_view_cap;

			Input::Session_component  _proxy_input;
			Input::Session_capability _proxy_input_cap;

			static long _session_arg(const char *arg, const char *key) {
				return Arg_string::find_arg(arg, key).long_value(0); }

		public:

			/**
			 * Constructor
			 */
			Session_component(Rpc_entrypoint           &ep,
			                  Signal_context_capability view_ready_sigh,
			                  const char               *args)
			:
				_ep(ep),

				/* store the framebuffer size for view size constraining */
				_fb_width(_session_arg(args, "fb_width")),
				_fb_height(_session_arg(args, "fb_height")),

				/* connect to the "real" Nitpicker service */
				_nitpicker(_fb_width, _fb_height),

				/* create Nitpicker view */
				_nitpicker_view(_nitpicker.create_view()),

				/* create proxy view component for the child */
				_proxy_view(_nitpicker_view, view_ready_sigh),
				_proxy_view_cap(_ep.manage(&_proxy_view)),

				/* create view component accessed by the loader client */
				_loader_view(_nitpicker_view),
				_loader_view_cap(_ep.manage(&_loader_view)),

				_proxy_input(_nitpicker.input_session(), *this),
				_proxy_input_cap(_ep.manage(&_proxy_input))
			{ }


			/*********************************
			 ** Nitpicker session interface **
			 *********************************/

			Framebuffer::Session_capability framebuffer_session()
			{
				return _nitpicker.framebuffer_session();
			}

			Input::Session_capability input_session()
			{
				return _proxy_input_cap;
			}

			View_capability create_view()
			{
				return _proxy_view_cap;
			}

			void destroy_view(View_capability view)
			{
				_nitpicker.destroy_view(_nitpicker_view);
			}

			int background(View_capability view)
			{
				/* not forwarding to real nitpicker session */
				return 0;
			}


			/**********************************
			 ** Input::Transformer interface **
			 **********************************/

			Input::Transformer::Delta delta()
			{
				/* translate mouse position to child's coordinate system */
				Input::Transformer::Delta result = {
					_loader_view.x() + _loader_view.buf_x() +
					_proxy_view.x()  +  _proxy_view.buf_x(),
					_loader_view.y() + _loader_view.buf_y() +
					_proxy_view.y()  +  _proxy_view.buf_y() };

				return result;
			}

			/**
			 * Return the client-specific wrapper view for the Nitpicker view
			 * showing the child content
			 */
			View_capability loader_view() { return _loader_view_cap; }

			/**
			 * Return geometry of loader view
			 */
			Loader::Session::View_geometry loader_view_geometry()
			{
				Loader::Session::View_geometry result = {
					min(_proxy_view.w(), _fb_width),
					min(_proxy_view.h(), _fb_height),
					_proxy_view.buf_x(),
					_proxy_view.buf_y() };

				return result;
			}
	};
}

#endif /* _NITPICKER_H_ */
