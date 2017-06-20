/*
 * \brief  Component-local stack area base address
 * \author Stefan Kalkowski
 * \date   2017-06-02
 */

/*
 * Copyright (C) 2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* base-internal includes */
#include <base/internal/stack_area.h>

Genode::addr_t Genode::stack_area_virtual_base() { return 0x40000000UL; }
