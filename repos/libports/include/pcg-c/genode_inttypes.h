/*
 * \brief   Integer definitons for PCG
 * \author  Emery Hemingway
 * \date    2017-08-18
 */

/*
 * Copyright (C) 2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef __INCLUDE__PCG_C__GENODE_INTTYPES_H_
#define __INCLUDE__PCG_C__GENODE_INTTYPES_H_

#include <base/fixed_stdint.h>

#ifndef _UINT8_T_DECLARED
typedef genode_uint8_t  uint8_t;
#define _UINT8_T_DECLARED
#endif

#ifndef _UINT16_T_DECLARED
typedef genode_uint16_t uint16_t;
#define _UINT16_T_DECLARED
#endif

#ifndef _UINT32_T_DECLARED
typedef genode_uint32_t uint32_t;
#define _UINT32_T_DECLARED
#endif

#ifndef _UINT64_T_DECLARED
typedef genode_uint64_t uint64_t;
#define _UINT64_T_DECLARED
#endif

#ifndef _INTPTR_T_DECLARED
typedef unsigned long intptr_t;
#define	_INTPTR_T_DECLARED
#endif

#endif
