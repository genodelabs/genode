/*
 * \brief  Lx_kit dma memory buffer
 * \author Stefan Kalkowski
 * \date   2021-03-25
 */

/*
 * Copyright (C) 2021 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

#ifndef _LX_KIT__DMA_BUFFER_H_
#define _LX_KIT__DMA_BUFFER_H_

#include <lx_kit/memory.h>
#include <platform_session/dma_buffer.h>

namespace Lx_kit { class Dma_buffer; }


class Lx_kit::Dma_buffer : Platform::Dma_buffer, public Lx_kit::Mem_allocator::Buffer
{
	public:

		using Platform::Dma_buffer::Dma_buffer;

		size_t dma_addr() const override {
			return Platform::Dma_buffer::dma_addr(); }

		size_t size() const override {
			return Platform::Dma_buffer::size(); }

		size_t virt_addr() const override {
			return (size_t) Platform::Dma_buffer::local_addr<void>(); }

		Dataspace_capability cap() override {
			return Platform::Dma_buffer::cap(); }
};

#endif /* _LX_KIT__DMA_BUFFER_H_ */
