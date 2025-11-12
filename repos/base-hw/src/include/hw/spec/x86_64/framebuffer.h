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

#ifndef _SRC__LIB__HW__SPEC__X86_64__FRAMEBUFFER_H
#define _SRC__LIB__HW__SPEC__X86_64__FRAMEBUFFER_H

#include <base/fixed_stdint.h>

namespace Hw { struct Framebuffer; }

struct Hw::Framebuffer
{
	Genode::uint64_t addr   { 0 };
	Genode::uint32_t pitch  { 0 };
	Genode::uint32_t width  { 0 };
	Genode::uint32_t height { 0 };
	Genode::uint8_t  bpp    { 0 };
	Genode::uint8_t  type   { 0 };
} __attribute__((packed));

#endif /* _SRC__LIB__HW__SPEC__X86_64__FRAMEBUFFER_H */
