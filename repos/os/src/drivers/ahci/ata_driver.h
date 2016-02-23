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


/**
 * Return data of 'identify_device' ATA command
 */
struct Identity : Genode::Mmio
{
	Identity(Genode::addr_t base) : Mmio(base) { }

	struct Serial_number : Register_array<0x14, 8, 20, 8> { };
	struct Model_number : Register_array<0x36, 8, 40, 8> { };

	struct Queue_depth : Register<0x96, 16>
	{
		struct Max_depth : Bitfield<0, 5> { };
	};

	struct Sata_caps   : Register<0x98, 16>
	{
		struct Ncq_support : Bitfield<8, 1> { };
	};

	struct Sector_count : Register<0xc8, 64> { };

	struct Logical_block  : Register<0xd4, 16>
	{
		struct Per_physical : Bitfield<0,  3> { }; /* 2^X logical per physical */
		struct Longer_512   : Bitfield<12, 1> { };
		struct Multiple     : Bitfield<13, 1> { }; /* multiple logical blocks per physical */
	};

	struct Logical_words : Register<0xea, 32> { }; /* words (16 bit) per logical block */

	struct Alignment : Register<0x1a2, 16>
	{
		struct Logical_offset : Bitfield<0, 14> { }; /* offset first logical block in physical */
	};

	void info()
	{
		char mn[Model_number::ITEMS + 1];
		get_device_number<Model_number>(mn);
		if (mn[0] != 0)
			PLOG("\t\tModel number: %s", mn);
		char sn[Serial_number::ITEMS + 1];
		get_device_number<Serial_number>(sn);
		if (sn[0] != 0)
			PLOG("\t\tSerial number: %s", sn);

		PLOG("\t\tqueue depth: %u ncq: %u",
		     read<Queue_depth::Max_depth>() + 1,
		     read<Sata_caps::Ncq_support>());
		PLOG("\t\tnumer of sectors: %llu", read<Sector_count>());
		PLOG("\t\tmultiple logical blocks per physical: %s",
		     read<Logical_block::Multiple>() ? "yes" : "no");
		PLOG("\t\tlogical blocks per physical: %u",
		     1U << read<Logical_block::Per_physical>());
		PLOG("\t\tlogical block size is above 512 byte: %s",
		     read<Logical_block::Longer_512>() ? "yes" : "no");
		PLOG("\t\twords (16bit) per logical block: %u",
		     read<Logical_words>());
		PLOG("\t\toffset of first logical block within physical: %u",
		     read<Alignment::Logical_offset>());
	}

	template <typename Device_number>
	void get_device_number(char output[Device_number::ITEMS + 1])
	{
		long j = 0;
		for (unsigned long i = 0; i < Device_number::ITEMS; i++) {
			char c = (char) (read<Device_number>(i ^ 1));
			if ((c == ' ') && (j == 0))
				continue;
			output[j++] = c;
		}

		output[j] = 0;

		while ((j > 0) && (output[--j] == ' '))
			output[j] = 0;
	}
};

/**
 * Commands to distinguish between ncq and non-ncq operation
 */
struct Io_command
{
	virtual void command(Port           &por,
	                     Command_table  &table,
	                     bool            read,
	                     Block::sector_t block_number,
	                     size_t          count,
	                     unsigned        slot) = 0;

	virtual void handle_irq(Port &port, Port::Is::access_t status) = 0;
};

struct Ncq_command : Io_command
{
	void command(Port           &port,
	             Command_table  &table,
	             bool            read,
	             Block::sector_t block_number,
	             size_t          count,
	             unsigned        slot) override
	{
		table.fis.fpdma(read, block_number, count, slot);
		/* set pending */
		port.write<Port::Sact>(1U << slot);
	}

	void handle_irq(Port &port, Port::Is::access_t status) override
	{
		/*
		 * Check for completions of other requests immediately
		 */
		while (Port::Is::Sdbs::get(status = port.read<Port::Is>()))
			port.ack_irq();
	}
};

struct Dma_ext_command : Io_command
{
	void command(Port           &port,
	             Command_table  &table,
	             bool            read,
	             Block::sector_t block_number,
	             size_t          count,
	             unsigned     /* slot */) override
	{
		table.fis.dma_ext(read, block_number, count);
	}

	void handle_irq(Port &port, Port::Is::access_t status) override
	{
		if (Port::Is::Dma_ext_irq::get(status))
			port.ack_irq();
	}
};


/**
 * Drivers using ncq- and non-ncq commands
 */
struct Ata_driver : Port_driver
{
	Genode::Lazy_volatile_object<Identity>   info;
	Io_command                              *io_cmd = nullptr;
	Block::Packet_descriptor                 pending[32];

	Ata_driver(Port &port, Signal_context_capability state_change)
	: Port_driver(port, state_change)
	{
		Port::init();
		identify_device();
	}

	~Ata_driver()
	{
		if (io_cmd)
			destroy (Genode::env()->heap(), io_cmd);
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

		/* set ATA command */
		io_cmd->command(*this, table, read, block_number, count, slot);

		/* set or clear write flag in command header */
		Command_header header(command_header_addr(slot));
		header.write<Command_header::Bits::W>(read ? 0 : 1);
		header.clear_byte_count();

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

		switch (state) {

		case IDENTIFY:

			if (Port::Is::Dss::get(status) || Port::Is::Pss::get(status)) {
				info.construct(device_info);
				if (verbose)
					info->info();

				check_device();
				if (ncq_support())
					io_cmd = new (Genode::env()->heap()) Ncq_command();
				else
					io_cmd = new (Genode::env()->heap()) Dma_ext_command();

				ack_irq();
			}
			break;

		case READY:

			io_cmd->handle_irq(*this, status);
			ack_packets();

		default:
			break;
		}

		stop();
	}

	bool ncq_support()
	{
		return info->read<Identity::Sata_caps::Ncq_support>() && hba.ncq();
	}

	void check_device()
	{
		cmd_slots = min((int)cmd_slots,
		                info->read<Identity::Queue_depth::Max_depth >() + 1);

		/* no native command queueing */
		if (!ncq_support())
			cmd_slots = 1;

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
