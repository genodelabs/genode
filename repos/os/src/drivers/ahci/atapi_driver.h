/*
 * \brief  AHCI-port driver for ATAPI devices
 * \author Sebastian Sumpf
 * \date   2015-04-29
 */

/*
 * Copyright (C) 2015 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _ATAPI_DRIVER_H_
#define _ATAPI_DRIVER_H_

#include "ahci.h"
#include <util/endian.h>

using namespace Genode;

struct Atapi_driver : Port_driver
{
	unsigned                 sense_tries = 0;
	Block::Packet_descriptor pending;

	Atapi_driver(Port &port, Ahci_root &root, unsigned &sem)
	: Port_driver(port, root, sem)
	{
		Port::init();
		Port::write<Cmd::Atapi>(1);
		read_sense();
	}

	void atapi_command()
	{
		Command_header header(command_header_addr(0));
		header.atapi_command();
		header.clear_byte_count();
		execute(0);
	}

	void test_unit_ready()
	{
		state = TEST_READY;

		Command_table table(command_table_addr(0), 0, 0);
		table.fis.atapi();
		table.atapi_cmd.test_unit_ready();

		atapi_command();
	}

	void read_sense()
	{
		state = STATUS;

		if (sense_tries++ >= 3) {
			Genode::error("could not power up device");
			state_change();
			return;
		}

		addr_t phys   = (addr_t)Dataspace_client(device_info_ds).phys_addr();

		Command_table table(command_table_addr(0), phys, 0x1000);
		table.fis.atapi();
		table.atapi_cmd.read_sense();

		atapi_command();
	}

	void read_capacity()
	{
		state       = IDENTIFY;
		addr_t phys = (addr_t)Dataspace_client(device_info_ds).phys_addr();

		Command_table table(command_table_addr(0), phys, 0x1000);
		table.fis.atapi();
		table.atapi_cmd.read_capacity();

		atapi_command();
	}

	void ack_packets()
	{
		unsigned slots = Port::read<Ci>();

		if (slots & 1 || !pending.size())
			return;

		Block::Packet_descriptor p = pending;
		pending  = Block::Packet_descriptor();
		ack_packet(p, true);
	}


	/*****************
	 ** Port_driver **
	 *****************/

	void handle_irq() override
	{
		Is::access_t status = Port::read<Is>();

		if (verbose) {
			Genode::log("irq: "
			            "is: ",    Genode::Hex(status), " "
			            "ci: ",    Genode::Hex(Port::read<Ci>()), " "
			            "state: ", (int)state);
			Device_fis f(fis_base);
			Genode::log("d2h: "
			            "status: ", f.read<Device_fis::Status>(), " "
			            "error: ", Genode::Hex(f.read<Device_fis::Error>()));
		}

		ack_irq();

		if (state == TEST_READY && Port::Is::Dhrs::get(status)) {
			Device_fis f(fis_base);

			/* check if devic is ready */
			if (f.read<Device_fis::Status::Device_ready>() && !f.read<Device_fis::Error>())
				read_capacity();
			else
				read_sense();
		}

		if (state == READY && Port::Is::Dhrs::get(status)) {
			ack_packets();
		}

		if (Port::Is::Dss::get(status) || Port::Is::Pss::get(status)) {
			switch (state) {

				case STATUS:
					test_unit_ready();
					break;

				case IDENTIFY:
					state = READY;
					state_change();
					break;

				case READY:
					ack_packets();
					return;

				default:
					break;
			}
		}
	}


	/*****************************
	 ** Block::Driver interface **
	 *****************************/

	bool dma_enabled() { return true; };

	Block::Session::Operations ops() override
	{
		Block::Session::Operations o;
		o.set_operation(Block::Packet_descriptor::READ);
		return o;
	}

	Genode::size_t block_size() override
	{
		return host_to_big_endian(((unsigned *)device_info)[1]);
	}

	Block::sector_t block_count() override
	{
		return host_to_big_endian(((unsigned *)device_info)[0]) + 1;
	}

	void read_dma(Block::sector_t           block_number,
	              size_t                    count,
	              addr_t                    phys,
	              Block::Packet_descriptor &packet) override
	{
		if (pending.size())
			throw Block::Driver::Request_congestion();

		sanity_check(block_number, count);

		pending = packet;

		if (verbose)
			Genode::log("add packet read ", block_number, " count ", count, " -> 0");

		/* setup fis */
		Command_table table(command_table_addr(0), phys, count * block_size());
		table.fis.atapi();

		/* setup atapi command */
		table.atapi_cmd.read10(block_number, count);

		/* set and clear write flag in command header */
		Command_header header(command_header_addr(0));
		header.write<Command_header::Bits::W>(0);
		header.clear_byte_count();

		/* set pending */
		execute(0);
	}
};

#endif /* _ATAPI_DRIVER_H_ */
