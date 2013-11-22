/*
 * \brief  Block-driver interface
 * \author Christian Helmuth
 * \author Sebastian Sumpf
 * \date   2011-05-23
 */

/*
 * Copyright (C) 2011-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__BLOCK__DRIVER_H_
#define _INCLUDE__BLOCK__DRIVER_H_

#include <base/exception.h>
#include <base/stdint.h>

#include <ram_session/ram_session.h>


namespace Block {

	/**
	 * Interface to be implemented by the device-specific driver code
	 */
	struct Driver
	{
		/**
		 * Exceptions
		 */
		class Io_error : public ::Genode::Exception { };

		/**
		 * Request block size for driver and medium
		 */
		virtual Genode::size_t block_size()  = 0;

		/**
		 * Request capacity of medium in blocks
		 */
		virtual Genode::size_t block_count() = 0;

		/**
		 * Read from medium
		 *
		 * \param block_number  number of first block to read
		 * \param block_count   number of blocks to read
		 * \param out_buffer    output buffer for read request
		 */
		virtual void read(Genode::size_t  block_number,
		                  Genode::size_t  block_count,
		                  char           *out_buffer) = 0;

		/**
		 * Write to medium
		 *
		 * \param block_number  number of first block to write
		 * \param block_count   number of blocks to write
		 * \param buffer        buffer for write request
		 */
		virtual void write(Genode::size_t  block_number,
		                   Genode::size_t  block_count,
		                   char const     *buffer) = 0;

		/**
		 * Read from medium using DMA
		 *
		 * \param block_number  number of first block to read
		 * \param block_count   number of blocks to read
		 * \param phys          phyiscal address of read buffer
		 */
		virtual void read_dma(Genode::size_t block_number,
		                      Genode::size_t block_count,
		                      Genode::addr_t phys) = 0;

		/**
		 * Write to medium using DMA
		 *
		 * \param block_number  number of first block to write
		 * \param block_count   number of blocks to write
		 * \param phys          physical address of write buffer
		 */
		virtual void write_dma(Genode::size_t  block_number,
		                       Genode::size_t  block_count,
		                       Genode::addr_t  phys) = 0;

		/**
		 * Check if DMA is enabled for driver
		 *
		 * \return  true if DMA is enabled, false otherwise
		 */
		virtual bool dma_enabled() = 0;

		/**
		 * Allocate buffer which is suitable for DMA.
		 */
		virtual Genode::Ram_dataspace_capability alloc_dma_buffer(Genode::size_t) = 0;

		/**
		 * Synchronize with with device.
		 */
		virtual void sync() = 0;
	};


	/**
	 * Interface for constructing the driver object
	 */
	struct Driver_factory
	{
		/**
		 * Construct new driver
		 */
		virtual Driver *create() = 0;

		/**
		 * Destroy driver
		 */
		virtual void destroy(Driver *driver) = 0;
	};
}

#endif /* _BLOCK__DRIVER_H_ */
