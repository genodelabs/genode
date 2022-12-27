/*
 * \brief  Utility to allocate and locally attach a DMA buffer
 * \author Norman Feske
 * \date   2022-02-02
 */

/*
 * Copyright (C) 2022 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__PLATFORM_SESSION__DMA_BUFFER_H_
#define _INCLUDE__PLATFORM_SESSION__DMA_BUFFER_H_

/* Genode includes */
#include <base/attached_dataspace.h>
#include <platform_session/connection.h>

class Platform::Dma_buffer : Noncopyable
{
	private:

		struct Allocation
		{
			Platform::Connection &platform;

			size_t const size;
			Cache  const cache;

			Ram_dataspace_capability _alloc() {
				return platform.alloc_dma_buffer(size, cache); }

			Ram_dataspace_capability cap = _alloc();

			addr_t const dma_addr = platform.dma_addr(cap);

			Allocation(Connection &platform, size_t size, Cache cache)
			: platform(platform), size(size), cache(cache) { }

			~Allocation() { platform.free_dma_buffer(cap); }

		} _allocation;

		Attached_dataspace _ds { _allocation.platform._env.rm(), _allocation.cap };

	public:

		/**
		 * Constructor
		 *
		 * \param platform  platform session used for the buffer allocation
		 * \param size      DMA buffer size in bytes
		 *
		 * \throw Out_of_ram
		 * \throw Out_of_caps
		 * \throw Region_map::Region_conflict
		 */
		Dma_buffer(Connection &platform, size_t size, Cache cache)
		:
			_allocation(platform, size, cache)
		{ }

		/**
		 * Return component-local base address
		 */
		template <typename T> T       *local_addr()       { return _ds.local_addr<T>(); }
		template <typename T> T const *local_addr() const { return _ds.local_addr<T>(); }

		/**
		 * Return bus address to be used for DMA operations
		 */
		addr_t dma_addr() const { return _allocation.dma_addr; }

		/**
		 * Return DMA-buffer size in bytes
		 */
		size_t size() const { return _allocation.size; }

		/**
		 * Return DMA-buffer as dataspace capability
		 */
		Dataspace_capability cap() { return _ds.cap(); }
};

#endif /* _INCLUDE__PLATFORM_SESSION__DMA_BUFFER_H_ */
