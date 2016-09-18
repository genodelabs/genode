/*
 * \brief  Some size definitions and macros needed by LwIP.
 * \author Stefan Kalkowski
 * \author Emery Hemingway
 * \date   2009-11-10
 */

/*
 * Copyright (C) 2009-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef __LWIP__ARCH__CC_H__
#define __LWIP__ARCH__CC_H__

#include <base/fixed_stdint.h>


#ifndef LWIP_RAND
genode_uint32_t genode_rand();
#define LWIP_RAND() genode_rand()
#endif


#ifndef LWIP_NO_STDDEF_H
#define LWIP_NO_STDDEF_H 1
typedef unsigned long size_t;
typedef          long ptrdiff_t;
#endif /* LWIP_NO_STDDEF_H */


#ifndef LWIP_NO_STDINT_H
#define LWIP_NO_STDINT_H 1
typedef genode_uint8_t    u8_t;
typedef genode_int8_t     s8_t;
typedef genode_uint16_t   u16_t;
typedef genode_int16_t    s16_t;
typedef genode_uint32_t   u32_t;
typedef genode_int32_t    s32_t;

typedef unsigned long     mem_ptr_t;
#endif /* LWIP_NO_STDINT_H */


#ifndef LWIP_NO_INTTYPES_H
#define LWIP_NO_INTTYPES_H 1
/* Define (sn)printf formatters */
#define U16_F "u" // we don't have hu
#define S16_F "d" // we don't have hd
#define X16_F "x" // we don't have hx
#define U32_F "u"
#define S32_F "d"
#define X32_F "x"
#endif /* LWIP_NO_INTTYPES_H */


#ifndef LWIP_PLATFORM_DIAG
void lwip_printf(const char *format, ...);
#define LWIP_PLATFORM_DIAG(x)   do { lwip_printf x; } while(0)
#endif /* LWIP_PLATFORM_DIAG */


#ifdef GENODE_RELEASE
#define LWIP_PLATFORM_ASSERT(x)
#else /* GENODE_RELEASE */
void lwip_platform_assert(char const* msg, char const *file, int line);
#define LWIP_PLATFORM_ASSERT(x)                           \
	do {                                                  \
		lwip_platform_assert(x, __FILE__, __LINE__);      \
	} while (0)
#endif /* GENODE_RELEASE */


#ifndef LWIP_NO_LIMITS_H
#define LWIP_NO_LIMITS_H 1
#endif


#define LWIP_SKIP_PACKING_CHECK 1
#define PACK_STRUCT_FIELD(x) x
#define PACK_STRUCT_STRUCT
#define PACK_STRUCT_BEGIN
#define PACK_STRUCT_END

#ifndef BYTE_ORDER
#define BYTE_ORDER LITTLE_ENDIAN
#endif /* BYTE_ORDER */


void genode_free(void *ptr);
void *genode_malloc(unsigned long size);
void *genode_calloc(unsigned long number, unsigned long size);

#define mem_clib_free   genode_free
#define mem_clib_malloc genode_malloc
#define mem_clib_calloc genode_calloc

#endif /* __LWIP__ARCH__CC_H__ */
