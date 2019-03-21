/*
 * \brief  CPU register macros for x86_64
 * \author Stefan Kalkowski
 * \date   2017-04-07
 */

/*
 * Copyright (C) 2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _SRC__LIB__HW__SPEC__X86_64__REGISTER_MACROS_H_
#define _SRC__LIB__HW__SPEC__X86_64__REGISTER_MACROS_H_

#include <util/register.h>

#define X86_64_CR_REGISTER(name, cr, ...) \
	struct name : Genode::Register<64> \
	{ \
		static access_t read() \
		{ \
			access_t v; \
			asm volatile ("mov %%" #cr ", %0" : "=r" (v) :: ); \
			return v; \
		} \
 \
		static void write(access_t const v) { \
			asm volatile ("mov %0, %%" #cr :: "r" (v) : ); } \
 \
		__VA_ARGS__; \
	};

#define X86_64_MSR_REGISTER(name, msr, ...) \
	struct name : Genode::Register<64> \
	{ \
		static access_t read() \
		{ \
			access_t low; \
			access_t high; \
			asm volatile ("rdmsr" : "=a" (low), "=d" (high) : "c" (msr)); \
			return (high << 32) | (low & ~0U); \
		} \
 \
		static void write(access_t const) { } \
 \
		__VA_ARGS__; \
	};

#endif /* _SRC__LIB__HW__SPEC__X86_64__REGISTER_MACROS_H_ */
