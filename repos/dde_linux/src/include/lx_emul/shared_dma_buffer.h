/*
 * \brief  Lx_emul support to allocate shared DMA buffer for Genode's C API
 * \author Stefan Kalkowski
 * \date   2022-03-02
 */

/*
 * Copyright (C) 2022 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

#ifndef _LX_EMUL__SHARED_DMA_BUFFER_H_
#define _LX_EMUL__SHARED_DMA_BUFFER_H_

#include <genode_c_api/base.h>

#ifdef __cplusplus
extern "C" {
#endif

struct genode_shared_dataspace *
lx_emul_shared_dma_buffer_allocate(unsigned long size);

void lx_emul_shared_dma_buffer_free(struct genode_shared_dataspace * ds);

void * lx_emul_shared_dma_buffer_virt_addr(struct genode_shared_dataspace * ds);

#ifdef __cplusplus
}
#endif

#endif /* _LX_EMUL__SHARED_DMA_BUFFER_H_ */
