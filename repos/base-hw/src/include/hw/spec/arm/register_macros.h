/*
 * \brief  CPU register macros for ARM
 * \author Stefan Kalkowski
 * \date   2017-02-02
 */

/*
 * Copyright (C) 2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _SRC__LIB__HW__SPEC__ARM__REGISTER_MACROS_H_
#define _SRC__LIB__HW__SPEC__ARM__REGISTER_MACROS_H_

#include <util/register.h>

#define ARM_CP15_REGISTER_32BIT(name, crn, crm, op1, op2, ...) \
	struct name : Genode::Register<32> \
	{ \
		static access_t read() \
		{ \
			access_t v; \
			asm volatile ("mrc p15, " #op1 ", %0, " #crn ", " #crm ", " #op2 \
			              : "=r" (v) :: ); \
			return v; \
		} \
 \
		static void write(access_t const v) { \
			asm volatile ("mcr p15, " #op1 ", %0, " #crn ", " #crm ", " #op2 \
			              :: "r" (v) : ); } \
 \
		__VA_ARGS__; \
	};

#define ARM_CP15_REGISTER_64BIT(name, cr, op, ...) \
	struct name : Genode::Register<64> \
	{ \
		static access_t read() \
		{ \
			Genode::uint32_t v0, v1; \
			asm volatile ("mrrc p15, " #op ", %0, %1, " #cr \
			              : "=r" (v0), "=r" (v1) :: ); \
			return (access_t) v0 | ((access_t) v1 << 32); \
		} \
 \
		static void write(access_t const v) { \
			asm volatile ("mcrr p15, " #op ", %0, %1, " #cr \
			              :: "r" (v), "r" (v >> 32) : ); } \
 \
		__VA_ARGS__; \
	};

#define ARM_BANKED_REGISTER(name, reg, ...) \
	struct name : Genode::Register<32> \
	{ \
		static access_t read() \
		{ \
			access_t v; \
			asm volatile ("mrs %0, " #reg : "=r" (v)); \
			return v; \
		} \
 \
		static void write(access_t const v) { \
			asm volatile ("msr " #reg ", %0" :: "r" (v)); } \
 \
		__VA_ARGS__; \
	};

#endif /* _SRC__LIB__HW__SPEC__ARM__REGISTER_MACROS_H_ */
