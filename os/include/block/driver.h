/*
 * \brief  Block-driver interface
 * \author Christian Helmuth
 * \author Sebastian Sumpf
 * \author Stefan kalkowski
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
#include <base/signal.h>

#include <ram_session/ram_session.h>
#include <block_session/block_session.h>

namespace Block {
	class Session_component;
	struct Driver;
	struct Driver_factory;
};


/**
 * Interface to be implemented by the device-specific driver code
 */
struct Block::Driver
{
	Session_component *session;

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
	virtual Block::sector_t block_count() = 0;

	/**
	 * Request operations supported by the device
	 */
	virtual Session::Operations ops() = 0;

	/**
	 * Read from medium
	 *
	 * \param block_number  number of first block to read
	 * \param block_count   number of blocks to read
	 * \param buffer        output buffer for read request
	 */
	virtual void read(sector_t           block_number,
	                  Genode::size_t     block_count,
	                  char *             buffer,
	                  Packet_descriptor &packet) {
		throw Io_error(); }

	/**
	 * Write to medium
	 *
	 * \param block_number  number of first block to write
	 * \param block_count   number of blocks to write
	 * \param buffer        buffer for write request
	 */
	virtual void write(sector_t           block_number,
	                   Genode::size_t     block_count,
	                   const char *       buffer,
	                   Packet_descriptor &packet) {
		throw Io_error(); }

	/**
	 * Read from medium using DMA
	 *
	 * \param block_number  number of first block to read
	 * \param block_count   number of blocks to read
	 * \param phys          phyiscal address of read buffer
	 */
	virtual void read_dma(sector_t           block_number,
	                      Genode::size_t     block_count,
	                      Genode::addr_t     phys,
	                      Packet_descriptor &packet) {
		throw Io_error(); }

	/**
	 * Write to medium using DMA
	 *
	 * \param block_number  number of first block to write
	 * \param block_count   number of blocks to write
	 * \param phys          physical address of write buffer
	 */
	virtual void write_dma(sector_t           block_number,
	                       Genode::size_t     block_count,
	                       Genode::addr_t     phys,
	                       Packet_descriptor &packet) {
		throw Io_error(); }

	/**
	 * Check if DMA is enabled for driver
	 *
	 * \return  true if DMA is enabled, false otherwise
	 */
	virtual bool dma_enabled() { return false; }

	/**
	 * Allocate buffer which is suitable for DMA.
	 */
	virtual Genode::Ram_dataspace_capability
	alloc_dma_buffer(Genode::size_t size) {
		return Genode::env()->ram_session()->alloc(size, false); }

	/**
	 * Synchronize with with device.
	 */
	virtual void sync() {}
};


/**
 * Interface for constructing the driver object
 */
struct Block::Driver_factory
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

#endif /* _BLOCK__DRIVER_H_ */
