/*
 * \brief  Common types
 * \author Norman Feske
 * \date   2009-09-11
 */

/*
 * Copyright (C) 2014-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _TYPES_H_
#define _TYPES_H_

/* Genode includes */
#include <gui_session/connection.h>
#include <util/misc_math.h>
#include <nitpicker_gfx/box_painter.h>
#include <nitpicker_gfx/texture_painter.h>
#include <base/attached_rom_dataspace.h>
#include <base/heap.h>
#include <os/pixel_alpha8.h>
#include <os/texture_rgb888.h>
#include <util/reconstructible.h>
#include <nitpicker_gfx/text_painter.h>
#include <libc/component.h>

namespace Menu_view {

	using namespace Genode;

	using Genode::size_t;

	using Point = Surface_base::Point;
	using Area  = Surface_base::Area;
	using Rect  = Surface_base::Rect;
}

#endif /* _TYPES_H_ */
