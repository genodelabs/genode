/*
 * \brief  Standard fixed-width integer types
 * \author Christian Helmuth
 * \author Norman Feske
 * \date   2006-05-10
 *
 * For additional information, please revisit the 32bit version of this file.
 */

/*
 * Copyright (C) 2006-2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__64BIT__BASE__FIXED_STDINT_H_
#define _INCLUDE__64BIT__BASE__FIXED_STDINT_H_


/*
 * Fixed-size types usable from both C and C++ programs
 */
typedef signed char        genode_int8_t;
typedef unsigned char      genode_uint8_t;
typedef signed short       genode_int16_t;
typedef unsigned short     genode_uint16_t;
typedef signed             genode_int32_t;
typedef unsigned           genode_uint32_t;
typedef signed long long   genode_int64_t;
typedef unsigned long long genode_uint64_t;


/*
 * Types residing within Genode's C++ namespace
 */
#ifdef __cplusplus
namespace Genode {
	typedef genode_int8_t     int8_t;
	typedef genode_uint8_t   uint8_t;
	typedef genode_int16_t   int16_t;
	typedef genode_uint16_t uint16_t;
	typedef genode_int32_t   int32_t;
	typedef genode_uint32_t uint32_t;
	typedef genode_int64_t   int64_t;
	typedef genode_uint64_t uint64_t;
}
#endif

#endif /* _INCLUDE__64BIT__BASE__FIXED_STDINT_H_ */
