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

#ifndef _L4__RE__C__DATASPACE_H_
#define _L4__RE__C__DATASPACE_H_

#include <l4/sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif

enum l4re_ds_map_flags {
	L4RE_DS_MAP_FLAG_RO = 0,
	L4RE_DS_MAP_FLAG_RW = 1,
};

typedef l4_cap_idx_t l4re_ds_t;

typedef struct {
	unsigned long size;
	unsigned long flags;
} l4re_ds_stats_t;

L4_CV int l4re_ds_map_region(const l4re_ds_t ds, l4_addr_t offset, unsigned long flags,
                            l4_addr_t min_addr, l4_addr_t max_addr);

L4_CV long l4re_ds_size(const l4re_ds_t ds);

L4_CV int l4re_ds_phys(const l4re_ds_t ds, l4_addr_t offset,
                       l4_addr_t *phys_addr, l4_size_t *phys_size);

L4_CV int l4re_ds_copy_in(const l4re_ds_t ds, l4_addr_t dst_offs, const l4re_ds_t src,
                          l4_addr_t src_offs, unsigned long size);

L4_CV int l4re_ds_info(const l4re_ds_t ds, l4re_ds_stats_t *stats);

#ifdef __cplusplus
}
#endif

#endif /* _L4__RE__C__DATASPACE_H_ */
