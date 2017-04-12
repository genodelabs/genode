/*
 * \brief  Functions to perform port I/O
 * \author Reto Buerki
 * \date   2015-03-06
 */

/*
 * Copyright (C) 2015-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _CORE__SPEC__X86_64__PORT_IO_H_
#define _CORE__SPEC__X86_64__PORT_IO_H_

#include <base/stdint.h>

namespace Genode
{
	/**
	 * Read byte from I/O port
	 */
	inline uint8_t inb(uint16_t port)
	{
		uint8_t res;
		asm volatile ("inb %w1, %0" : "=a"(res) : "Nd"(port));
		return res;
	}

	/**
	 * Write byte to I/O port
	 */
	inline void outb(uint16_t port, uint8_t val)
	{
		asm volatile ("outb %0, %w1" : : "a"(val), "Nd"(port));
	}
}

#endif /* _CORE__SPEC__X86_64__PORT_IO_H_ */
