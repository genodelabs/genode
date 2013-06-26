/*
 * \brief  Genode C API string related functions
 * \author Stefan Kalkowski
 * \date   2013-06-26
 */

/*
 * Copyright (C) 2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__GENODE__STRING_H_
#define _INCLUDE__GENODE__STRING_H_

#include <l4/sys/compiler.h>

#include <genode/linkage.h>

#ifdef __cplusplus
extern "C" {
#endif

	L4_CV void genode_memcpy(void* dst, void *src, unsigned long size);

#ifdef __cplusplus
}
#endif

#endif //_INCLUDE__GENODE__STRING_H_
