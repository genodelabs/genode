/*
 * \brief  Kernel-specific core's 'log' backend
 * \author Stefan Kalkowski
 * \date   2016-10-10
 */

/*
 * Copyright (C) 2016 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* core includes */
#include <core_log.h>

/* seL4 includes */
#include <sel4/arch/functions.h>
#include <sel4/arch/syscalls.h>

void Genode::Core_log::out(char const c) { seL4_DebugPutChar(c); }
