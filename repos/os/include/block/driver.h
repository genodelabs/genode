/*
 * \brief  Block-driver interface
 * \author Christian Helmuth
 * \author Sebastian Sumpf
 * \author Stefan kalkowski
 * \date   2011-05-23
 */

/*
 * Copyright (C) 2011-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__BLOCK__DRIVER_H_
#define _INCLUDE__BLOCK__DRIVER_H_

#include <base/exception.h>
#include <base/stdint.h>
#include <base/signal.h>
#include <base/ram_allocator.h>
#include <block_session/rpc_object.h>

namespace Block {
	class Driver_session_base;
	class Driver_session;
	class Driver;
	struct Driver_factory;
};


struct Block::Driver_session_base : Genode::Interface
{
	/**
	 * Acknowledges a packet processed by the driver to the client
	 *
	 * \param packet   the packet to acknowledge
	 * \param success  indicated whether the processing was successful
	 *
	 * \throw Ack_congestion
	 */
	virtual void ack_packet(Packet_descriptor &packet,
	                        bool success) = 0;
};


class Block::Driver_session : public Driver_session_base,
                              public Block::Session_rpc_object
{
	public:

		/**
		 * Constructor
		 *
		 * \param rm     region map of local address space, used to attach
		 *               the packet-stream buffer to the local address space
		 * \param tx_ds  dataspace used as communication buffer
		 *               for the tx packet stream
		 * \param ep     entry point used for packet-stream channel
		 */
		Driver_session(Genode::Region_map           &rm,
		               Genode::Dataspace_capability  tx_ds,
		               Genode::Rpc_entrypoint       &ep)
		: Session_rpc_object(rm, tx_ds, ep) { }
};


/**
 * Interface to be implemented by the device-specific driver code
 */
class Block::Driver : Genode::Interface
{
	private:

		/*
		 * Noncopyable
		 */
		Driver(Driver const &);
		Driver &operator = (Driver const &);

		Genode::Ram_allocator &_ram;

		Driver_session_base *_session = nullptr;

	public:

		/**
		 * Exceptions
		 */
		class Io_error           : public ::Genode::Exception { };
		class Request_congestion : public ::Genode::Exception { };

		/**
		 * Constructor
		 */
		Driver(Genode::Ram_allocator &ram) : _ram(ram) { }

		/**
		 * Destructor
		 */
		virtual ~Driver() { }

		/**
		 * Request block-device information
		 */
		virtual Session::Info info() const = 0;

		/**
		 * Read from medium
		 *
		 * \param block_number  number of first block to read
		 * \param block_count   number of blocks to read
		 * \param buffer        output buffer for read request
		 * \param packet        packet descriptor from the client
		 *
		 * \throw Request_congestion
		 *
		 * Note: should be overridden by DMA non-capable devices
		 */
		virtual void read(sector_t            /* block_number */,
		                  Genode::size_t      /* block_count */,
		                  char *              /* buffer */,
		                  Packet_descriptor & /* packet */) {
			throw Io_error(); }

		/**
		 * Write to medium
		 *
		 * \param block_number  number of first block to write
		 * \param block_count   number of blocks to write
		 * \param buffer        buffer for write request
		 * \param packet        packet descriptor from the client
		 *
		 * \throw Request_congestion
		 *
		 * Note: should be overridden by DMA non-capable, non-ROM devices
		 */
		virtual void write(sector_t            /* block_number */,
		                   Genode::size_t      /* block_count */,
		                   const char *        /* buffer */,
		                   Packet_descriptor & /* packet */) {
			throw Io_error(); }

		/**
		 * Read from medium using DMA
		 *
		 * \param block_number  number of first block to read
		 * \param block_count   number of blocks to read
		 * \param phys          phyiscal address of read buffer
		 * \param packet        packet descriptor from the client
		 *
		 * \throw Request_congestion
		 *
		 * Note: should be overridden by DMA capable devices
		 */
		virtual void read_dma(sector_t            /* block_number */,
		                      Genode::size_t      /* block_count */,
		                      Genode::addr_t      /* phys */,
		                      Packet_descriptor & /* packet */) {
			throw Io_error(); }

		/**
		 * Write to medium using DMA
		 *
		 * \param block_number  number of first block to write
		 * \param block_count   number of blocks to write
		 * \param phys          physical address of write buffer
		 * \param packet        packet descriptor from the client
		 *
		 * \throw Request_congestion
		 *
		 * Note: should be overridden by DMA capable, non-ROM devices
		 */
		virtual void write_dma(sector_t            /* block_number */,
		                       Genode::size_t      /* block_count */,
		                       Genode::addr_t      /* phys */,
		                       Packet_descriptor & /* packet */) {
			throw Io_error(); }

		/**
		 * Check if DMA is enabled for driver
		 *
		 * \return  true if DMA is enabled, false otherwise
		 *
		 * Note: has to be overriden by DMA-capable devices
		 */
		virtual bool dma_enabled() { return false; }

		/**
		 * Allocate buffer which is suitable for DMA.
		 *
		 * Note: has to be overriden by DMA-capable devices
		 */
		virtual Genode::Ram_dataspace_capability
		alloc_dma_buffer(Genode::size_t size) {
			return _ram.alloc(size); }

		/**
		 * Free buffer which is suitable for DMA.
		 *
		 * Note: has to be overriden by DMA-capable devices
		 */
		virtual void free_dma_buffer(Genode::Ram_dataspace_capability c) {
			return _ram.free(c); }

		/**
		 * Synchronize with device.
		 *
		 * Note: should be overriden by (e.g. intermediate) components,
		 *       which cache data
		 */
		virtual void sync() {}

		/**
		 * Informs the driver that the client session was closed
		 *
		 * Note: drivers with state (e.g. asynchronously working)
		 *       should override this method, and reset their internal state
		 */
		virtual void session_invalidated() { }

		/**
		 * Set single session component of the driver
		 *
		 * Session might get used to acknowledge requests.
		 */
		void session(Driver_session_base *session) {
			if (!(_session = session)) session_invalidated(); }

		/**
		 * Acknowledge a packet after processing finished to the client
		 *
		 * \param p  packet to acknowledge
		 */
		void ack_packet(Packet_descriptor &p, bool success = true) {
			if (_session) _session->ack_packet(p, success); }
};


/**
 * Interface for constructing the driver object
 */
struct Block::Driver_factory : Genode::Interface
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
