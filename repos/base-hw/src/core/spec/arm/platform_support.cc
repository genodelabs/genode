/*
 * \brief   ARM specific platform implementations
 * \author  Adrian-Ken Rueegsegger
 * \date    2015-03-18
 */

/*
 * Copyright (C) 2015 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* core includes */
#include <platform.h>

using namespace Genode;

void Platform::_init_io_port_alloc() { };


long Platform::irq(long const user_irq) { return user_irq; }