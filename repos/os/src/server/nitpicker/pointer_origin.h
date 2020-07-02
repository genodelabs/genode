/*
 * \brief  View that represents the origin of the pointer coordinate system
 * \author Norman Feske
 * \date   2014-07-02
 */

/*
 * Copyright (C) 2014-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _POINTER_ORIGIN_H_
#define _POINTER_ORIGIN_H_

#include "view.h"
#include "gui_session.h"

namespace Nitpicker { struct Pointer_origin; }


struct Nitpicker::Pointer_origin : View
{
	Resizeable_texture<Pixel> _texture { };

	Pointer_origin(View_owner &owner)
	:
		View(owner, _texture, View::TRANSPARENT, View::NOT_BACKGROUND, 0)
	{ }


	/********************
	 ** View interface **
	 ********************/

	int  frame_size(Focus const &) const override { return 0; }
	void frame(Canvas_base &, Focus const &) const override { }
	void draw(Canvas_base &, Font const &, Focus const &) const override { }
};

#endif /* _POINTER_ORIGIN_H_ */
