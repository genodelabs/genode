/*
 * \brief  Kernel-specific implementations for Versatile Express with TZ
 * \author Stefan Kalkowski
 * \date   2012-10-11
 */

/*
 * Copyright (C) 2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Core includes */
#include <kernel_support.h>

Cpu::User_context::User_context() {
	cpsr = Psr::init_user_with_trustzone(); }

