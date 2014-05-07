/*
 * \brief  L4Re functions needed by L4Linux
 * \author Stefan Kalkowski
 * \date   2011-03-17
 */

/*
 * Copyright (C) 2011-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _L4__RE__C__MEM_ALLOC_H_
#define _L4__RE__C__MEM_ALLOC_H_

#include <l4/re/env.h>
#include <l4/sys/consts.h>

#include <l4/re/c/dataspace.h>

#ifdef __cplusplus
extern "C" {
#endif

enum l4re_ma_flags {
	L4RE_MA_CONTINUOUS  = 0x01,
	L4RE_MA_PINNED      = 0x02,
	L4RE_MA_SUPER_PAGES = 0x04,
};

L4_CV long l4re_ma_alloc(unsigned long size, l4re_ds_t const mem,
                         unsigned long flags);

L4_CV long l4re_ma_alloc_align(unsigned long size, l4re_ds_t const mem,
                               unsigned long flags, unsigned long align);

L4_CV long l4re_ma_free(l4re_ds_t const mem);

#ifdef __cplusplus
}
#endif

#endif /* _L4__RE__C__MEM_ALLOC_H_ */
