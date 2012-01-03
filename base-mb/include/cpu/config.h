/*
 * \brief  Configuration of underlying hardware
 * \author Martin stein
 * \date   07-05-2010
 */

/*
 * Copyright (C) 07-2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__CPU__CONFIG_H_
#define _INCLUDE__CPU__CONFIG_H_

#define ALWAYS_INLINE __attribute__((always_inline))

#define BITFIELD_ENUMS(name, bit_significancy_offset, bit_width) \
	name ## _LSH = bit_significancy_offset, \
	name ## _WID = bit_width, \
	name ## _MSK = ~((~0) << bit_width) << bit_significancy_offset,

namespace Cpu {

	typedef unsigned char  uint8_t;
	typedef unsigned short uint16_t;
	typedef unsigned int   uint32_t;

	typedef uint8_t  byte_t;
	typedef uint32_t word_t;

	typedef unsigned long addr_t;
	typedef __SIZE_TYPE__ size_t;

	enum {
		BYTE_WIDTH_LOG2 = 3,
		WORD_WIDTH_LOG2 = 5,
		BYTE_WIDTH      = 1 << BYTE_WIDTH_LOG2,
		WORD_WIDTH      = 1 << WORD_WIDTH_LOG2,
		BYTE_SIZE       = sizeof(byte_t),
		WORD_SIZE       = sizeof(word_t),

		_16B_SIZE_LOG2   = 1*WORD_SIZE,
		_256B_SIZE_LOG2  = 2*WORD_SIZE,
		_4KB_SIZE_LOG2   = 3*WORD_SIZE,
		_64KB_SIZE_LOG2  = 4*WORD_SIZE,
		_1MB_SIZE_LOG2   = 5*WORD_SIZE,
		_16MB_SIZE_LOG2  = 6*WORD_SIZE,
		_256MB_SIZE_LOG2 = 7*WORD_SIZE,

		_16B_SIZE   = 1 <<   _16B_SIZE_LOG2,
		_256B_SIZE  = 1 <<  _256B_SIZE_LOG2,
		_4KB_SIZE   = 1 <<   _4KB_SIZE_LOG2,
		_64KB_SIZE  = 1 <<  _64KB_SIZE_LOG2,
		_1MB_SIZE   = 1 <<   _1MB_SIZE_LOG2,
		_16MB_SIZE  = 1 <<  _16MB_SIZE_LOG2,
		_256MB_SIZE = 1 << _256MB_SIZE_LOG2,
	};

	enum {
		RAM_BASE = 0x90000000,
		RAM_SIZE = 0x06000000,

		XPS_INTC_BASE  = 0x81800000,

		XPS_TIMER_0_BASE = 0x83c00000,
		XPS_TIMER_0_IRQ  = 0,

		XPS_ETHERNETLITE_BASE = 0x81000000,
		XPS_ETHERNETLITE_IRQ  = 1,

		XPS_UARTLITE_BASE = 0x84000000,
		XPS_UARTLITE_IRQ  = 3,

		XPS_TIMER_1_BASE = 0x70000000,
		XPS_TIMER_1_IRQ  = 4,
	};

	typedef uint8_t Irq_id;
	typedef uint8_t Exception_id;

	enum {
		FAST_SIMPLEX_LINK      = 0,
		UNALIGNED              = 1,
		ILLEGAL_OPCODE         = 2,
		INSTRUCTION_BUS        = 3,
		DATA_BUS               = 4,
		DIV_BY_ZERO_EXCEPTON   = 5,
		FPU                    = 6,
		PRIVILEGED_INSTRUCTION = 7,

		INTERRUPT                   = 10,
		EXTERNAL_NON_MASKABLE_BREAK = 11,
		EXTERNAL_MASKABLE_BREAK     = 12,

		DATA_STORAGE         = 16,
		INSTRUCTION_STORAGE  = 17,
		DATA_TLB_MISS        = 18,
		INSTRUCTION_TLB_MISS = 19,

		MIN_EXCEPTION_ID = 0,
		MAX_EXCEPTION_ID = 19,

		INVALID_EXCEPTION_ID = 20
	};

	enum {
		MIN_IRQ_ID = 0,
		MAX_IRQ_ID = 31,

		INVALID_IRQ_ID = 32,
	};
}

#endif /* _INCLUDE__CPU__CONFIG_H_ */
