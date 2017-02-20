/*
 * \brief   Color definitions used by nitpicker
 * \date    2006-08-04
 * \author  Norman Feske
 */

/*
 * Copyright (C) 2006-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _COLOR_H_
#define _COLOR_H_

#include <util/color.h>

/*
 * Symbolic names for some important colors
 */
static const Genode::Color BLACK(0, 0, 0);
static const Genode::Color WHITE(255, 255, 255);
static const Genode::Color FRAME_COLOR(255, 200, 127);
static const Genode::Color KILL_COLOR(255, 0, 0);

#endif /* _COLOR_H_ */
