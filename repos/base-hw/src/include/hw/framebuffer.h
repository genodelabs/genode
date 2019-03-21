/*
 * \brief   Framebuffer structure as provided by bootloader
 * \author  Alexander Boettcher
 * \date    2017-11-20
 */

/*
 * Copyright (C) 2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _SRC__LIB__HW__FRAMEBUFFER_H
#define _SRC__LIB__HW__FRAMEBUFFER_H

#include <base/fixed_stdint.h>

namespace Hw {
	struct Framebuffer;
}

struct Hw::Framebuffer
{
    Genode::uint64_t addr;
    Genode::uint32_t pitch;
    Genode::uint32_t width;
    Genode::uint32_t height;
    Genode::uint8_t  bpp;
    Genode::uint8_t  type;
} __attribute__((packed));

#endif /* _SRC__LIB__HW__FRAMEBUFFER_H */
