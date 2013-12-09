/*
 * \brief  AHCI driver declaration
 * \author Martin Stein <martin.stein@genode-labs.com>
 * \date   2013-04-10
 */

/*
 * Copyright (C) 2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _AHCI_DRIVER_H_
#define _AHCI_DRIVER_H_

/* Genode includes */
#include <block/component.h>
#include <ram_session/ram_session.h>

/**
 * AHCI driver
 */
class Ahci_driver : public Block::Driver
{
	enum { VERBOSE = 0 };

	/* import Genode symbols */
	typedef Genode::size_t size_t;
	typedef Genode::uint64_t uint64_t;
	typedef Genode::addr_t addr_t;

	int _ncq_command(uint64_t lba, unsigned cnt, addr_t phys, bool w);

	public:

		/**
		 * Constructor
		 */
		Ahci_driver();


		/*****************************
		 ** Block::Driver interface **
		 *****************************/

		Block::Session::Operations ops()
		{
			Block::Session::Operations o;
			o.set_operation(Block::Packet_descriptor::READ);
			o.set_operation(Block::Packet_descriptor::WRITE);
			return o;
		}

		size_t          block_size();
		Block::sector_t block_count();
		bool   dma_enabled() { return true; }

		void read_dma(Block::sector_t block_nr, size_t block_cnt, addr_t phys,
		              Block::Packet_descriptor &packet)
		{
			if (_ncq_command(block_nr, block_cnt, phys, 0))
				throw Io_error();
			session->ack_packet(packet);
		}

		void write_dma(Block::sector_t block_nr, size_t  block_cnt, addr_t  phys,
		               Block::Packet_descriptor &packet)
		{
			if (_ncq_command(block_nr, block_cnt, phys, 1))
				throw Io_error();
			session->ack_packet(packet);
		}
};

#endif /* _AHCI_DRIVER_H_ */

