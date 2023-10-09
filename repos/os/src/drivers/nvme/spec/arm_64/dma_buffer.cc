/*
 * \brief  ARM 64bit DMA buffer
 * \author Johannes Schlatow
 * \date   2023-10-09
 */

/*
 * Copyright (C) 2023 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* local includes */
#include <dma_buffer.h>

Util::Dma_buffer::Dma_buffer(Platform::Connection &platform, Genode::size_t size)
: Platform::Dma_buffer(platform, size, Genode::UNCACHED)
{ }
