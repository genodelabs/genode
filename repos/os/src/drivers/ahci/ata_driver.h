/*
 * \brief  AHCI-port driver for ATA devices
 * \author Sebastian Sumpf
 * \date   2015-04-29
 */

/*
 * Copyright (C) 2015-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _ATA_DRIVER_H_
#define _ATA_DRIVER_H_

#include <base/log.h>
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
		using Genode::log;

		log("  queue depth: ", read<Queue_depth::Max_depth>() + 1, " "
		    "ncq: ", read<Sata_caps::Ncq_support>());
		log("  numer of sectors: ", read<Sector_count>());
		log("  multiple logical blocks per physical: ",
		    read<Logical_block::Multiple>() ? "yes" : "no");
		log("  logical blocks per physical: ",
		    1U << read<Logical_block::Per_physical>());
		log("  logical block size is above 512 byte: ",
		    read<Logical_block::Longer_512>() ? "yes" : "no");
		log("  words (16bit) per logical block: ",
		    read<Logical_words>());
		log("  offset of first logical block within physical: ",
		    read<Alignment::Logical_offset>());
	}
};


/**
 * 16-bit word big endian device ASCII characters
 */
template <typename DEVICE_STRING>
struct String
{
	char buf[DEVICE_STRING::ITEMS + 1];

	String(Identity & info)
	{
		long j = 0;
		for (unsigned long i = 0; i < DEVICE_STRING::ITEMS; i++) {
			/* read and swap even and uneven characters */
			char c = (char)info.read<DEVICE_STRING>(i ^ 1);
			if (Genode::is_whitespace(c) && j == 0)
				continue;
			buf[j++] = c;
		}

		buf[j] = 0;

		/* remove trailing white spaces */
		while ((j > 0) && (buf[--j] == ' '))
			buf[j] = 0;
	}

	bool operator == (char const *other) const
	{
		return strcmp(buf, other) == 0;
	}

	void print(Genode::Output &out) const { Genode::print(out, (char const *)buf); }
	char const *cstring() { return buf; }
};


/**
 * Commands to distinguish between ncq and non-ncq operation
 */
struct Io_command : Interface
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
	void command(Port           &,
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
	Genode::Allocator &alloc;

	typedef ::String<Identity::Serial_number> Serial_string;
	typedef ::String<Identity::Model_number>  Model_string;

	Genode::Constructible<Identity>      info   { };
	Genode::Constructible<Serial_string> serial { };
	Genode::Constructible<Model_string>  model  { };

	Io_command               *io_cmd = nullptr;
	Block::Packet_descriptor  pending[32];

	Signal_context_capability device_identified;

	Ata_driver(Genode::Allocator   &alloc,
	           Genode::Ram_session &ram,
	           Ahci_root           &root,
	           unsigned            &sem,
	           Genode::Region_map  &rm,
	           Hba                 &hba,
	           Platform::Hba       &platform_hba,
	           unsigned             number,
	           Genode::Signal_context_capability device_identified)
	: Port_driver(ram, root, sem, rm, hba, platform_hba, number),
	  alloc(alloc), device_identified(device_identified)
	{
		Port::init();
		identify_device();
	}

	~Ata_driver()
	{
		if (io_cmd)
			destroy(&alloc, io_cmd);
	}

	unsigned find_free_cmd_slot()
	{
		for (unsigned slot = 0; slot < cmd_slots; slot++)
			if (!pending[slot].size())
				return slot;

		throw Block::Driver::Request_congestion();
	}

	void ack_packets()
	{
		unsigned slots =  Port::read<Ci>() | Port::read<Sact>();

		for (unsigned slot = 0; slot < cmd_slots; slot++) {
			if ((slots & (1U << slot)) || !pending[slot].size())
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
			if (!pending[slot].size())
				continue;

			Block::sector_t pending_start = pending[slot].block_number();
			Block::sector_t pending_end   = pending_start + pending[slot].block_count() - 1;
			/* check if a pending packet overlaps */
			if ((block_number >= pending_start && block_number <= pending_end) ||
			    (end          >= pending_start && end          <= pending_end) ||
			    (pending_start >= block_number && pending_start <= end) ||
			    (pending_end   >= block_number && pending_end   <= end)) {

				Genode::warning("overlap: "
				                "pending ", pending[slot].block_number(),
				                " + ", pending[slot].block_count(), ", "
				                "request: ", block_number, " + ", count);
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

		switch (state) {

		case IDENTIFY:

			if (Port::Is::Dss::get(status)
			    || Port::Is::Pss::get(status)
			    || Port::Is::Dhrs::get(status)) {
				info.construct(device_info);
				serial.construct(*info);
				model.construct(*info);

				if (verbose) {
					Genode::log("  model number: ",  Genode::Cstring(model->buf));
					Genode::log("  serial number: ", Genode::Cstring(serial->buf));
					info->info();
				}

				check_device();
				if (ncq_support())
					io_cmd = new (&alloc) Ncq_command();
				else
					io_cmd = new (&alloc) Dma_ext_command();

				ack_irq();

				Genode::Signal_transmitter(device_identified).submit();
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

	private:

		/*
		 * Noncopyable
		 */
		Ata_driver(Ata_driver const &);
		Ata_driver &operator = (Ata_driver const &);
};


#endif /* _ATA_DRIVER_H_ */
