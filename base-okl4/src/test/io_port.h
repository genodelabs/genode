/*
 * \brief  I/O port access function for x86
 * \author Norman Feske
 * \date   2009-03-31
 */

/*
 * Copyright (C) 2009-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _IO_PORT_H_
#define _IO_PORT_H_

/**
 * Read byte from I/O port
 */
inline unsigned char inb(unsigned short port)
{
	unsigned char res;
	asm volatile ("inb %%dx, %0" :"=a"(res) :"Nd"(port));
	return res;
}


/**
 * Write byte to I/O port
 */
inline void outb(unsigned short port, unsigned char val)
{
	asm volatile ("outb %b0, %w1" : : "a" (val), "Nd" (port));
}

#endif /* _IO_PORT_H_ */
