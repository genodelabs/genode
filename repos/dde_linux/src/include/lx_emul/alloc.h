/*
 * \brief  Lx_emul support to allocate memory
 * \author Stefan Kalkowski
 * \date   2021-03-25
 */

/*
 * Copyright (C) 2021 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

#ifndef _LX_EMUL__ALLOC_H_
#define _LX_EMUL__ALLOC_H_

#ifdef __cplusplus
extern "C" {
#endif

void * lx_emul_mem_alloc_aligned(unsigned long size, unsigned long align);
void * lx_emul_mem_alloc_aligned_uncached(unsigned long size, unsigned long align);
unsigned long lx_emul_mem_dma_addr(void * addr);
unsigned long lx_emul_mem_virt_addr(void * dma_addr);
void   lx_emul_mem_free(const void * ptr);
unsigned long lx_emul_mem_size(const void * ptr);
void  lx_emul_mem_cache_clean_invalidate(const void * ptr, unsigned long size);
void  lx_emul_mem_cache_invalidate(const void * ptr, unsigned long size);

#ifdef __cplusplus
}
#endif

#endif /* _LX_EMUL__ALLOC_H_ */
