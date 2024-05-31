/*
 * \brief  DMA buffer utility
 * \author Johannes Schlatow
 * \date   2023-10-09
 */

/*
 * Copyright (C) 2023 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _NVME_DMA_BUFFER_H_
#define _NVME_DMA_BUFFER_H_

/* Genode includes */
#include <platform_session/dma_buffer.h>

namespace Util {

	using namespace Genode;

	/**
	 * We use an arch-specific Dma_buffer implementation to care about
	 * whether to use CACHED or UNCACHED memory.
	 */
	class Dma_buffer : public Platform::Dma_buffer
	{
		public:

			Dma_buffer(Platform::Connection &platform, size_t size);

		/**
		 * If needed, we can add a method for cache clean/invalidate operations
		 * that arch-specifically. Currently, the driver is used on x86 with
		 * strong coherency and arm with uncached memory.
		 */
	};
}

#endif /* _NVME_DMA_BUFFER_H_ */
