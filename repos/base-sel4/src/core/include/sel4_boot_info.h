/*
 * \brief   Access to seL4 boot info
 * \author  Norman Feske
 * \date    2015-05-04
 */

/*
 * Copyright (C) 2015-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _CORE__INCLUDE__SEL4_BOOT_INFO_H_
#define _CORE__INCLUDE__SEL4_BOOT_INFO_H_

/* seL4 includes */
#include <sel4/bootinfo.h>

namespace Genode { seL4_BootInfo const &sel4_boot_info(); }

#endif /* _CORE__INCLUDE__SEL4_BOOT_INFO_H_ */
