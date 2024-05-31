/*
 * \brief  PL180-specific implementation of the Block::Driver interface
 * \author Christian Helmuth
 * \author Martin Stein
 * \date   2011-05-19
 */

/*
 * Copyright (C) 2011-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* local includes */
#include <driver.h>

using namespace Genode;
using namespace Sd_card;


void Driver::_write_command(unsigned cmd_index, bool resp)
{
	enum Bits {
		CmdIndex = 0x3f,
		Response = 1 << 6,
		Enable   = 1 << 10
	};
	cmd_index  = (cmd_index & CmdIndex) | Enable;
	cmd_index |= (resp ? Response : 0);

	_write_reg(Command, cmd_index);

	while (!(_read_reg(Status) & (CmdRespEnd | CmdSent))) ;
}


void Driver::_request(unsigned char  cmd,
                      unsigned      *out_resp)
{
	_write_reg(Argument, 0);
	_write_command(cmd, (out_resp != 0));
	if (out_resp)
		*out_resp = _read_reg(Response0);
	_clear_status();
}


void Driver::_request(unsigned char  cmd,
                      unsigned       arg,
                      unsigned      *out_resp)
{
	_write_reg(Argument, arg);
	_write_command(cmd, (out_resp != 0));
	if (out_resp)
		*out_resp = _read_reg(Response0);
	_clear_status();
}


void Driver::_read_request(unsigned char  cmd,
                           unsigned       arg,
                           unsigned       length,
                           unsigned      *out_resp)
{
	/*
	 * FIXME on real hardware the blocksize must be written into
	 * DataCtrl:BlockSize.
	 */
	enum { CTRL_ENABLE = 0x01, CTRL_READ = 0x02 };

	_write_reg(DataLength, length);
	_write_reg(DataCtrl, CTRL_ENABLE | CTRL_READ);

	_write_reg(Argument, arg);
	_write_command(cmd, (out_resp != 0));
	if (out_resp)
		*out_resp = _read_reg(Response0);
	_clear_status();
}


void Driver::_write_request(unsigned char  cmd,
                            unsigned       arg,
                            unsigned       length,
                            unsigned      *out_resp)
{
	/*
	 * FIXME on real hardware the blocksize must be written into
	 * DataCtrl:BlockSize.
	 */
	enum { CTRL_ENABLE = 0x01 };

	_write_reg(DataLength, length);
	_write_reg(DataCtrl, CTRL_ENABLE);

	_write_reg(Argument, arg);
	_write_command(cmd, (out_resp != 0));
	if (out_resp)
		*out_resp = _read_reg(Response0);
	_clear_status();
}


void Driver::_read_data(unsigned  length,
                        char     *out_buffer)
{
	unsigned *buf = reinterpret_cast<unsigned *>(out_buffer);
	for (unsigned count = 0; count < length / 4; ) {
		/*
		 * The FIFO contains at least 'remainder - FifoCnt' words.
		 */
		int chunk  = length / 4 - count - _read_reg(FifoCnt);
		count     += chunk;
		for ( ; chunk > 0; --chunk)
			buf[count - chunk] = _read_reg(FIFO);
	}
	_clear_status();
}


void Driver::_write_data(unsigned    length,
                         char const *buffer)
{
	enum { FIFO_SIZE = 16 };

	unsigned const *buf = reinterpret_cast<unsigned const *>(buffer);
	for (unsigned count = 0; count < length / 4; ) {
		uint32_t status;
		while (!((status = _read_reg(Status)) & TxFifoHalfEmpty)) ;

		int chunk  = (status & TxFifoEmpty) ? FIFO_SIZE : FIFO_SIZE / 2;
		count     += chunk;
		for ( ; chunk > 0; --chunk)
			_write_reg(FIFO, buf[count - chunk]);
	}
	_clear_status();
}


Driver::Driver(Env &env, Platform::Connection & platform)
:
	Block::Driver(env.ram()),
	Platform::Device(platform),
	Platform::Device::Mmio<SIZE>(*this, { 0 }),
	_platform(platform),
	_timer(env)
{
	enum { POWER_UP = 2, POWER_ON = 3 };

	_write_reg(Power, POWER_UP);
	_timer.msleep(10);
	_write_reg(Power, POWER_ON);
	_timer.msleep(10);
	_clear_status();

	/* CMD0: go idle state */
	_request(0, 0);

	/*
	 * CMD8: send interface condition
	 *
	 * XXX only one hard-coded value currently.
	 */
	unsigned resp;
	_request(8, 0x1aa, &resp);

	/*
	 * ACMD41: card send operating condition
	 *
	 * This is an application-specific command and, therefore, consists
	 * of prefix command CMD55 + CMD41.
	 */
	_request(55, 0, &resp);
	_request(41, 0x4000, &resp);

	/* CMD2: all send card identification (CID) */
	_request(2, &resp);

	/* CMD3: send relative card address (RCA) */
	_request(3, &resp);
	unsigned short rca = (short)(resp >> 16);

	/*
	 * Now, the card is in transfer mode...
	 */

	/* CMD7: select card */
	_request(7, rca << 16, &resp);
}


Driver::~Driver() { }


void Driver::read(Block::sector_t           block_number,
                  size_t                    block_count,
                  char                     *buffer,
                  Block::Packet_descriptor &packet)
{
	unsigned resp;
	unsigned length = _block_size;

	for (size_t i = 0; i < block_count; ++i) {
		/*
		 * CMD17: read single block
		 *
		 * SDSC cards use a byte address as argument while SDHC/SDSC uses a
		 * block address here.
		 */
		_read_request(17, (uint32_t)((block_number + i) * _block_size),
		              length, &resp);
		_read_data(length, buffer + (i * _block_size));
	}
	ack_packet(packet);
}


void Driver::write(Block::sector_t           block_number,
                   size_t                    block_count,
                   char const               *buffer,
                   Block::Packet_descriptor &packet)
{
	unsigned resp;
	unsigned length = _block_size;

	for (size_t i = 0; i < block_count; ++i) {
		/*
		 * CMD24: write single block
		 *
		 * SDSC cards use a byte address as argument while SDHC/SDSC uses a
		 * block address here.
		 */
		_write_request(24, (uint32_t)((block_number + i) * _block_size),
		               length, &resp);
		_write_data(length, buffer + (i * _block_size));
	}
	ack_packet(packet);
}

