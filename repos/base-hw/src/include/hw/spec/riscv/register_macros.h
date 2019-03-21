/*
 * \brief  CPU register macros for RiscV
 * \author Stefan Kalkowski
 * \date   2017-09-14
 */

/*
 * Copyright (C) 2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _SRC__LIB__HW__SPEC__RISCV__REGISTER_MACROS_H_
#define _SRC__LIB__HW__SPEC__RISCV__REGISTER_MACROS_H_

#include <util/register.h>

#define RISCV_SUPERVISOR_REGISTER(name, reg, ...) \
	struct name : Genode::Register<64> \
	{ \
		static access_t read() \
		{ \
			access_t v; \
			asm volatile ("csrr %0, " #reg : "=r" (v) :: ); \
			return v; \
		} \
 \
		static void write(access_t const v) { \
			asm volatile ("csrw " #reg ", %0" :: "r" (v) : ); } \
 \
		__VA_ARGS__; \
	};

#endif /* _SRC__LIB__HW__SPEC__ARM__REGISTER_MACROS_H_ */

