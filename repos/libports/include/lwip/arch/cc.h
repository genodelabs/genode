/*
 * \brief  Some size definitions and macros needed by LwIP.
 * \author Stefan Kalkowski
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

#include <stddef.h>

#include <base/fixed_stdint.h>

typedef genode_uint8_t    u8_t;
typedef genode_int8_t     s8_t;
typedef genode_uint16_t   u16_t;
typedef genode_int16_t    s16_t;
typedef genode_uint32_t   u32_t;
typedef genode_int32_t    s32_t;

typedef unsigned long     mem_ptr_t;

/* Define (sn)printf formatters */
#define U16_F "u" // we don't have hu
#define S16_F "d" // we don't have hd
#define X16_F "x" // we don't have hx
#define U32_F "u"
#define S32_F "d"
#define X32_F "x"

void lwip_printf(const char *format, ...);
void lwip_sleep_forever(void);

#define LWIP_PLATFORM_DIAG(x)   do { lwip_printf x; } while(0)

#ifdef GENODE_RELEASE
#define LWIP_PLATFORM_ASSERT(x)
#else /* GENODE_RELEASE */
#define LWIP_PLATFORM_ASSERT(x)                           \
	do {                                                  \
		lwip_printf("Assertion \"%s\" failed at %s:%u\n", \
		            x, __FILE__, __LINE__);               \
		lwip_sleep_forever();                             \
	} while (0)
#endif /* GENODE_RELEASE */

#define PACK_STRUCT_FIELD(x) x
#define PACK_STRUCT_STRUCT
#define PACK_STRUCT_BEGIN
#define PACK_STRUCT_END

#ifndef BYTE_ORDER
#define BYTE_ORDER LITTLE_ENDIAN
#endif /* BYTE_ORDER */

#define mem_realloc realloc

#endif /* __LWIP__ARCH__CC_H__ */
