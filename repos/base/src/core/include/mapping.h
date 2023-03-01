/*
 * \brief  Kernel-agnostic memory-mapping arguments
 * \author Norman Feske
 * \date   2021-04-12
 */

/*
 * Copyright (C) 2021 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _CORE__INCLUDE__MAPPING_H_
#define _CORE__INCLUDE__MAPPING_H_

/* core includes */
#include <types.h>

namespace Core { struct Mapping; }


struct Core::Mapping
{
	addr_t dst_addr;
	addr_t src_addr;
	size_t size_log2;
	bool   cached;           /* RAM caching policy */
	bool   io_mem;           /* IO_MEM dataspace */
	bool   dma_buffer;       /* must be mapped in IOMMU page tables */
	bool   write_combined;   /* write-combined IO_MEM dataspace */
	bool   writeable;
	bool   executable;

	void prepare_map_operation() const;
};

#endif /* _CORE__INCLUDE__MAPPING_H_ */
