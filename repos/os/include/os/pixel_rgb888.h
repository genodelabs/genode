/*
 * \brief  Template specializations for the RGB888 pixel format
 * \author Norman Feske
 * \date   2014-08-14
 */

/*
 * Copyright (C) 2014 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__OS__PIXEL_RGB888_H_
#define _INCLUDE__OS__PIXEL_RGB888_H_

#include <os/pixel_rgba.h>

namespace Genode {

	typedef Pixel_rgba<unsigned long, Surface_base::RGB888,
	                  0xff0000, 16, 0xff00, 8, 0xff, 0, 0, 0>
	        Pixel_rgb888;
}

#endif /* _INCLUDE__OS__PIXEL_RGB888_H_ */
