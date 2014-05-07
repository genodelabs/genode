/*
 * \brief  Nitpicker view interface
 * \author Norman Feske
 * \date   2006-08-10
 */

/*
 * Copyright (C) 2006-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__NITPICKER_VIEW__NITPICKER_VIEW_H_
#define _INCLUDE__NITPICKER_VIEW__NITPICKER_VIEW_H_

#include <nitpicker_session/nitpicker_session.h>
#include <nitpicker_view/capability.h>
#include <base/rpc.h>
#include <base/rpc_args.h>

namespace Nitpicker {

	struct View
	{
		virtual ~View() { }

		/**
		 * Define position and viewport
		 *
		 * Both attributes are handled in one function to enable atomic
		 * updates of position and viewport. This is the common case for
		 * moving an overlay window.
		 */
		virtual int viewport(int x, int y, int w, int h,
		                     int buf_x, int buf_y, bool redraw = true) = 0;

		/**
		 * Reposition view in view stack
		 *
		 * \param neighbor  neighbor view
		 * \param behind    insert view in front (true) or
		 *                  behind (false) the specified neighbor
		 * \param redraw    redraw affected screen region
		 *
		 * To insert a view at the top of the view stack, specify
		 * neighbor = invalid and behind = true. To insert a view at the
		 * bottom of the view stack, specify neighbor = invalid and
		 * behind = false.
		 */
		virtual int stack(View_capability neighbor = View_capability(),
		                  bool behind = true, bool redraw = true) = 0;

		/**
		 * String type used as argument for the 'title' function
		 */
		typedef Genode::Rpc_in_buffer<64> Title;

		/**
		 * Assign new view title
		 */
		virtual int title(Title const &title) = 0;


		/*********************
		 ** RPC declaration **
		 *********************/

		GENODE_RPC(Rpc_viewport, int, viewport, int, int, int, int, int, int, bool);
		GENODE_RPC(Rpc_stack, int, stack, View_capability, bool, bool);
		GENODE_RPC(Rpc_title, int, title, Title const &);

		GENODE_RPC_INTERFACE(Rpc_viewport, Rpc_stack, Rpc_title);
	};
}

#endif /* _INCLUDE__NITPICKER_VIEW__NITPICKER_VIEW_H_ */
