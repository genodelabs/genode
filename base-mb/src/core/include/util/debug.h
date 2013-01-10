/*
 * \brief  Some tools for general purpose debugging
 * \author Martin stein
 * \date   2011-04-06
 */

/*
 * Copyright (C) 2011-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__UTIL__DEBUG_H_
#define _INCLUDE__UTIL__DEBUG_H_

#include <cpu/config.h>
#include <base/printf.h>

namespace Debug {

	/**
	 * Print out address and the according 32 bit memory-value
	 * XXX Should print a word instead of a fixed bitwidth XXX
	 */
	inline void dump(Cpu::addr_t const & a);

	/**
	 * Print memory-contents of a given area over the local addressspace
	 * as list with the according addresses in front
	 */
	inline void dump(Cpu::addr_t const & base, Cpu::size_t const & size,
			  bool downward = false);
};


void Debug::dump(Cpu::addr_t const & a) {
	printf("%8X: %8X", (Cpu::uint32_t)a, *((Cpu::uint32_t*)a));
}


void Debug::dump(Cpu::addr_t const & base, Cpu::size_t const & size,
                 bool downward)
{
	using namespace Genode;
	Cpu::addr_t top = base + size;

	if(!downward) {
		for(Cpu::addr_t i=base; i<top;) {
			dump(i);
			i = i+Cpu::WORD_SIZE;
		}
		return;
	}
	for(Cpu::addr_t i=top; i>=base;) {
		i = i-Cpu::WORD_SIZE;
		dump(i);
	}
}

#endif /* _INCLUDE__UTIL__DEBUG_H_ */

