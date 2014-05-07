/*
 * \brief  Client-side nitpicker view interface
 * \author Norman Feske
 * \date   2006-08-23
 */

/*
 * Copyright (C) 2006-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__NITPICKER_VIEW__CLIENT_H_
#define _INCLUDE__NITPICKER_VIEW__CLIENT_H_

#include <nitpicker_view/nitpicker_view.h>
#include <base/rpc_client.h>

namespace Nitpicker {

	struct View_client : public Genode::Rpc_client<View>
	{
		explicit View_client(View_capability view)
		: Genode::Rpc_client<View>(view) { }

		int viewport(int x, int y, int w, int h,
		             int buf_x, int buf_y, bool redraw) {
			return call<Rpc_viewport>(x, y, w, h, buf_x, buf_y, redraw); }

		int stack(View_capability neighbor, bool behind, bool redraw) {
			return call<Rpc_stack>(neighbor, behind, redraw); }

		int title(Title const &title) {
			return call<Rpc_title>(title); }
	};
}

#endif /* _INCLUDE__NITPICKER_VIEW__CLIENT_H_ */
