/*
 * \brief  ATAPI specific implementation of an Ata::Device
 * \author Sebastian.Sumpf <sebastian.sumpf@genode-labs.com>
 * \date   2010-07-19
 */

/*
 * Copyright (C) 2010-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <base/env.h>
#include <base/printf.h>
#include <util/string.h>

/* local includes */
#include "ata_device.h"
#include "ata_bus_master.h"
#include "endian.h"
#include "mindrvr.h"

static const int verbose = 0;

using namespace Ata;
using namespace Genode;

enum Commands {
	CMD_READ_10         = 0x28,
	CMD_READ_CAPACITY   = 0x25,
	CMD_REQUEST_SENSE   = 0x03,
	CMD_TEST_UNIT_READY = 0x00,
};


Atapi_device::Atapi_device(unsigned base_cmd, unsigned base_ctrl)
  : Device(base_cmd, base_ctrl)
{ }


int Atapi_device::read_sense(unsigned char *sense, int length)
{
		unsigned char cmd_sense[12];
		
		memset(cmd_sense, 0, 12);
		memset(sense, 0, length);

		cmd_sense[0] = CMD_REQUEST_SENSE;
		cmd_sense[4] = length; /* allocation length */
		return reg_packet(dev_num(), 12, cmd_sense, 0, length, sense);
}


bool Atapi_device::test_unit_ready(int level)
{
	unsigned char cmd[12];

	memset(cmd, 0, 12);
	cmd[0] = CMD_TEST_UNIT_READY;

	if (reg_packet(dev_num(), 12, cmd, 0, 0, 0)) {
		unsigned char sense[32];
		read_sense(sense, 32);

		PERR("Sense: key %x sub-key %x", sense[2], sense[12]);
		if (level < 3)
			return test_unit_ready(level + 1);
		else {
			return false;
		}
	}

	return true;
}


void Atapi_device::read_capacity()
{
	unsigned char cmd[12];
	unsigned buffer[2];
	memset(cmd, 0, 12);
	memset(buffer, 0, 8);

	cmd[0] = CMD_READ_CAPACITY;
	
	if (!test_unit_ready())
		throw Io_error();

	if (reg_packet(dev_num(), 12, cmd, 0, 8, (unsigned char *)buffer))
		throw Io_error();

	_block_start = 0;
	_block_end   = bswap32(buffer[0]);
	_block_size  = bswap32(buffer[1]);

	if (verbose)
		PDBG("First block: %u last block %u, block size %u",
		     _block_start, _block_end, _block_size);
}


void Atapi_device::_read(Genode::size_t  block_nr,
                         Genode::size_t  count,
                         char           *buffer,
                         bool            dma)
{
	unsigned char cmd[12];
	memset(cmd, 0, 12);

	cmd[0] = CMD_READ_10;

	block_nr += _block_start;

	if (block_nr < _block_start || block_nr + count > _block_end + 1)
		throw Io_error();

	/* block address */
	cmd[2] = (block_nr >> 24) & 0xff;
	cmd[3] = (block_nr >> 16) & 0xff;
	cmd[4] = (block_nr >> 8)  & 0xff;
	cmd[5] =  block_nr & 0xff;

	/* transfer length */
	cmd[7] = (count >> 8) & 0xff;
	cmd[8] = count & 0xff;

	if (dma) {
		if (verbose)
			PDBG("DMA read: block %zu, count %zu, buffer: %p",
			     block_nr, count, (void*)buffer);

		if (dma_pci_packet(dev_num(), 12, cmd, 0, count * _block_size,
		                   (unsigned char*)buffer))
			throw Io_error();
	} else {
		if (reg_packet(dev_num(), 12, cmd, 0, count * _block_size,
		               (unsigned char*)buffer))
			throw Io_error();
	}
}


void Atapi_device::_write(Genode::size_t  block_number,
                          Genode::size_t  block_count,
                          char const     *buffer,
                          bool            dma) { throw Io_error(); }


Block::Session::Operations Atapi_device::ops()
{
	Block::Session::Operations o;
	o.set_operation(Block::Packet_descriptor::READ);
	return o;
}
