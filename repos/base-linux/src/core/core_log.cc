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

/* Linux syscall bindings */
#include <linux_syscalls.h>

void Genode::Core_log::out(char const c) { lx_write(1, &c, sizeof(c)); }
