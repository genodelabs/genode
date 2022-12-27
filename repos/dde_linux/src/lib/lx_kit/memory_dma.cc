/*
 * \brief  Lx_kit DMA-capable memory allocation backend
 * \author Stefan Kalkowski
 * \date   2021-03-25
 */

/*
 * Copyright (C) 2021 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

/* Genode includes */
#include <util/touch.h>

/* local includes */
#include <lx_kit/dma_buffer.h>
#include <lx_kit/memory.h>


Lx_kit::Mem_allocator::Buffer &
Lx_kit::Mem_allocator::alloc_buffer(size_t size)
{
	size = align_addr(size, 12);

	Buffer & buffer = *static_cast<Buffer*>(new (_heap)
		Lx_kit::Dma_buffer(_platform, size, _cache_attr));

	/* map eager by touching all pages once */
	for (size_t sz = 0; sz < buffer.size(); sz += 4096) {
		touch_read((unsigned char const volatile*)(buffer.virt_addr() + sz)); }

	_virt_to_dma.insert(buffer.virt_addr(), buffer);
	_dma_to_virt.insert(buffer.dma_addr(),  buffer);
	return buffer;
}
