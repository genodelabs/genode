/*
 * \brief  Board driver for core
 * \author Norman Feske
 * \date   2013-04-05
 */

/*
 * Copyright (C) 2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _RPI__BOARD_H_
#define _RPI__BOARD_H_

/* Genode includes */
#include <drivers/board_base.h>

namespace Genode { struct Board; }

struct Genode::Board : Genode::Board_base { static void prepare_kernel() { } };

#endif /* _RPI__BOARD_H_ */
