#ifndef _PORT_IO_H_
#define _PORT_IO_H_

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

#endif /* _PORT_IO_H_ */
