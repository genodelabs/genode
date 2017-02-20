/*
 * \brief  Minimal std*.h and string.h include for libports/contrib/x86emu-*
 * \author Alexander Boettcher <alexander.boettcher@genode-labs.com>
 * \date   2013-06-07
 */

/*
 * Copyright (C) 2013-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <base/fixed_stdint.h>

typedef genode_int8_t     int8_t;
typedef genode_uint8_t   uint8_t;
typedef genode_int16_t   int16_t;
typedef genode_uint16_t uint16_t;
typedef genode_int32_t   int32_t;
typedef genode_uint32_t uint32_t;
typedef genode_int64_t   int64_t;
typedef genode_uint64_t uint64_t;

int abs(int j);

#ifndef NULL
#define NULL (void*)0
#endif
