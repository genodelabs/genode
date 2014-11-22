/**
 * \brief  Platform specific types
 * \author Josef Soentgen
 * \date   2014-10-17
 */

/*
 * Copyright (C) 2014 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _X86_64__TYPES_H_
#define _X86_64__TYPES_H_

typedef signed char        int8_t;
typedef unsigned char      uint8_t;
typedef short              int16_t;
typedef unsigned short     uint16_t;
typedef int                int32_t;
typedef unsigned int       uint32_t;
typedef __SIZE_TYPE__      size_t;
typedef long long          int64_t;
typedef unsigned long long uint64_t;

#endif /* _X86_64__TYPES_H_ */
