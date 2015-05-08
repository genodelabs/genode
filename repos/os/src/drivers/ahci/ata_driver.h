/**
 * \brief  AHCI-port driver for ATA devices
 * \author Sebastian Sumpf
 * \date   2015-04-29
 */

/*
 * Copyright (C) 2015 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _ATA_DRIVER_H_
#define _ATA_DRIVER_H_

#include "ahci.h"

using namespace Genode;

struct Ata_driver : Port_driver
{
	Genode::Lazy_volatile_object<Identity> info;
	Block::Packet_descriptor               pending[32];

	Ata_driver(Port &port, Signal_context_capability state_change)
	: Port_driver(port, state_change)
	{
		Port::init();
		identify_device();
	}

	unsigned find_free_cmd_slot()
	{
		for (unsigned slot = 0; slot < cmd_slots; slot++)
			if (!pending[slot].valid())
				return slot;

		throw Block::Driver::Request_congestion();
	}

	void ack_packets()
	{
		unsigned slots =  Port::read<Ci>() | Port::read<Sact>();

		for (unsigned slot = 0; slot < cmd_slots; slot++) {
			if ((slots & (1U << slot)) || !pending[slot].valid())
				continue;

			Block::Packet_descriptor p = pending[slot];
			pending[slot] = Block::Packet_descriptor();
			ack_packet(p, true);
		}
	}

	void overlap_check(Block::sector_t block_number,
	                   size_t          count)
	{
		Block::sector_t end = block_number + count - 1;

		for (unsigned slot = 0; slot < cmd_slots; slot++) {
			if (!pending[slot].valid())
				continue;

			Block::sector_t pending_start = pending[slot].block_number();
			Block::sector_t pending_end   = pending_start + pending[slot].block_count() - 1;
			/* check if a pending packet overlaps */
			if ((block_number >= pending_start && block_number <= pending_end) ||
			    (end          >= pending_start && end          <= pending_end) ||
			    (pending_start >= block_number && pending_start <= end) ||
			    (pending_end   >= block_number && pending_end   <= end)) {
				PWRN("Overlap: pending %llu + %zu, request: %llu + %zu", pending[slot].block_number(),
				     pending[slot].block_count(), block_number, count);
				throw Block::Driver::Request_congestion();
			}
		}
	}

	void io(bool                      read,
	        Block::sector_t           block_number,
	        size_t                    count,
	        addr_t                    phys,
	        Block::Packet_descriptor &packet)
	{
		sanity_check(block_number, count);
		overlap_check(block_number, count);

		unsigned slot = find_free_cmd_slot();
		pending[slot] = packet;

		/* setup fis */
		Command_table table(command_table_addr(slot), phys, count * block_size());
		table.fis.fpdma(read, block_number, count, slot);

		/* set or clear write flag in command header */
		Command_header header(command_header_addr(slot));
		header.write<Command_header::Bits::W>(read ? 0 : 1);
		header.clear_byte_count();

		/* set pending */
		Port::write<Sact>(1U << slot);
		execute(slot);
	}


	/*****************
	 ** Port_driver **
	 *****************/

	void handle_irq() override
	{
		Is::access_t status = Port::read<Is>();

		if (verbose)
			PDBG("irq: %x state: %u", status, state);

		if ((Port::Is::Dss::get(status) || Port::Is::Pss::get(status)) &&
		    state == IDENTIFY) {
			info.construct(device_info);
			if (verbose)
				info->info();

			check_device();
		}

		/*
		 * When packets get acked, new ones may be inserted by the block driver
		 * layer, these new packet requests might be finished during 'ack_packets',
		 * so clear the IRQ and re-read after 'ack_packets'
		 */
		while (Is::Sdbs::get(status = Port::read<Is>())) {
			ack_irq();
			ack_packets();
		}

		stop();
	}

	void check_device()
	{
		if (!info->read<Identity::Sata_caps::Ncq_support>() ||
		    !hba.ncg()) {
			PERR("Device does not support native command queuing: abort");
			state_change();
			return;
		}

		cmd_slots = min((int)cmd_slots,
		                info->read<Identity::Queue_depth::Max_depth >() + 1);
		state = READY;
		state_change();
	}

	void identify_device()
	{
		state       = IDENTIFY;
		addr_t phys = (addr_t)Dataspace_client(device_info_ds).phys_addr();

		Command_table table(command_table_addr(0), phys, 0x1000);
		table.fis.identify_device();
		execute(0);
	}


	/*****************************
	 ** Block::Driver interface **
	 *****************************/

	bool dma_enabled() { return true; };

	Block::Session::Operations ops() override
	{
		Block::Session::Operations o;
		o.set_operation(Block::Packet_descriptor::READ);
		o.set_operation(Block::Packet_descriptor::WRITE);
		return o;
	}

	void read_dma(Block::sector_t           block_number,
	              size_t                    block_count,
	              addr_t                    phys,
	              Block::Packet_descriptor &packet) override
	{
		io(true, block_number, block_count, phys, packet);
	}

	void write_dma(Block::sector_t           block_number,
	               size_t                    block_count,
	               addr_t                    phys,
	               Block::Packet_descriptor &packet) override
	{
		io(false, block_number, block_count, phys, packet);
	}

	Genode::size_t block_size() override
	{
		Genode::size_t size = 512;

		if (info->read<Identity::Logical_block::Longer_512>())
			size = info->read<Identity::Logical_words>() / 2;

		return size;
	}

	Block::sector_t block_count() override
	{
		return info->read<Identity::Sector_count>();
	}
};


#endif /* _ATA_DRIVER_H_ */
