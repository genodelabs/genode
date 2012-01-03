/*
 * \brief  Hexdump utility
 * \author Julian Stecklina
 * \date   2008-02-20
 */

/*
 * Copyright (C) 2008-2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__UTIL__HEXDUMP_H_
#define _INCLUDE__UTIL__HEXDUMP_H_

namespace Util {

	/**
	 * Dump a block of memory in a nice way to the terminal.
	 *
	 * \param addr the memory address to start dump
	 * \param length the amount of bytes to be dumped
	 */
	void hexdump(const unsigned char *addr,
	             unsigned long        length);

	/**
	 * Exactly like hexdump, but prints real_addr instead of addr as address
	 *
	 * \param addr the memory address to start dump
	 * \param length the amount of bytes to be dumped
	 */
	void hexdump(const unsigned char *addr,
	             unsigned long        length,
	             unsigned long        real_addr);
}

#endif /* _INCLUDE__UTIL__HEXDUMP_H_ */
