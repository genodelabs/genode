/*
 * \brief  Menu view
 * \author Norman Feske
 * \date   2009-09-11
 */

/*
 * Copyright (C) 2014 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _TYPES_H_
#define _TYPES_H_

/* Genode includes */
#include <nitpicker_session/connection.h>
#include <base/printf.h>
#include <util/misc_math.h>
#include <os/config.h>
#include <decorator/xml_utils.h>
#include <nitpicker_gfx/box_painter.h>
#include <nitpicker_gfx/texture_painter.h>
#include <os/attached_dataspace.h>
#include <os/attached_rom_dataspace.h>
#include <os/pixel_rgb565.h>
#include <os/pixel_alpha8.h>
#include <os/texture_rgb888.h>
#include <util/volatile_object.h>
#include <nitpicker_gfx/text_painter.h>

namespace Menu_view {

	using namespace Genode;

	using Genode::size_t;

	typedef Surface_base::Point Point;
	typedef Surface_base::Area  Area;
	typedef Surface_base::Rect  Rect;
}

#endif /* _TYPES_H_ */
