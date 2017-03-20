/*
 * \brief  Exynos5-specific implementation of the Block::Driver interface
 * \author Sebastian Sumpf
 * \author Martin Stein
 * \date   2013-03-22
 */

/*
 * Copyright (C) 2015-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* local includes */
#include <driver.h>

using namespace Genode;
using namespace Sd_card;


Driver::Driver(Env &env)
:
	Driver_base(env.ram()),
	Attached_mmio(env, MSH_BASE, MSH_SIZE), _env(env)
{
	_irq.sigh(_irq_handler);
	_irq.ack_irq();

	log("SD/MMC card detected");
	log("capacity: ", _card_info.capacity_mb(), " MiB");
}


void Driver::read_dma(Block::sector_t           block_number,
                      size_t                    block_count,
                      addr_t                    buf_phys,
                      Block::Packet_descriptor &pkt)
{
	if (_block_transfer.pending) {
		throw Request_congestion(); }

	if (!_setup_idmac_descriptor_table(block_count, buf_phys))
		throw Io_error();

	_block_transfer.packet  = pkt;
	_block_transfer.pending = true;

	if (!_issue_command(Read_multiple_block(block_number))) {
		error("Read_multiple_block failed");
		throw Io_error();
	}
}


void Driver::write_dma(Block::sector_t           block_number,
                       size_t                    block_count,
                       addr_t                    buf_phys,
                       Block::Packet_descriptor &pkt)
{
	if (_block_transfer.pending) {
		throw Request_congestion(); }

	if (!_setup_idmac_descriptor_table(block_count, buf_phys))
		throw Io_error();

	_block_transfer.packet  = pkt;
	_block_transfer.pending = true;

	if (!_issue_command(Write_multiple_block(block_number))) {
		error("Read_multiple_block failed");
		throw Io_error();
	}
}


bool Driver::_reset()
{
	Mmio::write<Ctrl::Reset>(0x7);
	try { wait_for(Attempts(100), Microseconds(1000), _delayer,
	               Ctrl::Reset::Equal(0)); }
	catch (Polling_timeout) {
		error("Could not reset host contoller");
		return false;
	}
	return true;
}


void Driver::_reset_fifo()
{
	Mmio::write<Ctrl::Reset>(0x2);
	try { wait_for(Attempts(100), Microseconds(1000), _delayer,
	               Ctrl::Reset::Equal(0)); }
	catch (Polling_timeout) {
		error("Could not reset fifo"); }
}


void Driver::_disable_irq()
{
	Mmio::write<Rintsts>(~0U);
	Mmio::write<Intmask>(0);
}


bool Driver::_update_clock_registers()
{
	Cmd::access_t cmd = 0;
	Cmd::Wait_prvdata_complete::set(cmd, 1);
	Cmd::Update_clock_registers_only::set(cmd, 1);
	Cmd::Start_cmd::set(cmd, 1);
	Mmio::write<Cmd>(cmd);

	try { wait_for(_delayer, Cmd::Start_cmd::Equal(0)); }
	catch (Polling_timeout) {
		error("Update clock registers failed");
		return false;
	}
	return true;
}


bool Driver::_setup_bus(unsigned clock_div)
{
	/* set host clock divider */
	Mmio::write<Clkdiv>(clock_div);

	if (!_update_clock_registers())
		return false;

	/* enable clock for card 1 */
	Mmio::write<Clkena>(0x1);

	if (!_update_clock_registers())
		return false;

	_delayer.usleep(10 * 1000);
	return true;
}


Card_info Driver::_init()
{
	Mmio::write<Pwren>(1);
	if (!_reset())
		throw Detection_failed();

	Mmio::write<Emmc_ddr_req>(0x1);

	_disable_irq();

	Mmio::write<Tmout>(~0U);
	Mmio::write<Idinten>(0);
	Mmio::write<Bmod>(1);
	Mmio::write<Bytcnt>(0);
	Mmio::write<Fifoth>(0x203f0040);

	/* set to one bit transfer Bit */
	if (!_setup_bus(CLK_DIV_400Khz))
		throw Detection_failed();

	Mmio::write<Ctype>(BUS_WIDTH_1);

	if (!issue_command(Go_idle_state())) {
		warning("Go_idle_state command failed");
		throw Detection_failed();
	}
	_delayer.usleep(2000);

	if (!issue_command(Send_if_cond())) {
		warning("Send_if_cond command failed");
		throw Detection_failed();
	}
	 /* if this succeeds it is an SD card */
	if ((Mmio::read<Rsp0>() & 0xff) == 0xaa)
		log("Found SD card");

	/*
	 * We need to issue the same Mmc_send_op_cond command multiple
	 * times. The first time, we receive the status information. On
	 * subsequent attempts, the response tells us that the card is
	 * busy. Usually, the command is issued twice. We give up if the
	 * card is not reaching busy state after one second.
	 */
	unsigned i = 1000;
	unsigned voltages = 0x300080;
	unsigned arg = 0;
	for (; i > 0; --i) {
		if (!issue_command(Mmc_send_op_cond(arg, true))) {
			warning("Sd_send_op_cond command failed");
			throw Detection_failed();
		}
		arg = Mmio::read<Rsp0>();
		arg = (voltages & (arg & 0x007FFF80)) | (arg & 0x60000000);

		_delayer.usleep(1000);

		if (Ocr::Busy::get(Mmio::read<Rsp0>()))
			break;
	}
	if (i == 0) {
		error("Send_op_cond timed out, could no power-on SD/MMC card");
		throw Detection_failed();
	}
	Card_info card_info = _detect_mmc();

	/* switch frequency to high speed */
	enum { EXT_CSD_HS_TIMING = 185 };
	if (!issue_command(Mmc_switch(EXT_CSD_HS_TIMING, 1))) {
		error("Error setting high speed frequency");
		throw Detection_failed();
	}
	enum { EXT_CSD_BUS_WIDTH = 183 };
	/* set card to 8 bit */
	if (!issue_command(Mmc_switch(EXT_CSD_BUS_WIDTH, 2))) {
		error("Error setting card bus width");
		throw Detection_failed();
	}
	Mmio::write<Ctype>(BUS_WIDTH_8);

	/* set to eight bit transfer Bit */
	if (!_setup_bus(CLK_DIV_52Mhz)) {
		error("Error setting bus to high speed");
		throw Detection_failed();
	}
	/* Enable IRQs data read timeout, data transfer done, resp error */
	Mmio::write<Intmask>(0x28a);
	Mmio::write<Ctrl::Global_interrupt>(1);
	return card_info;
}


bool Driver::_setup_idmac_descriptor_table(size_t block_count,
                                           addr_t phys_addr)
{
	size_t const max_idmac_block_count = IDMAC_DESC_MAX_ENTRIES * 8;

	if (block_count > max_idmac_block_count) {
		error("Block request too large");
		return false;
	}
	_reset_fifo();

	Idmac_desc::Flags flags = Idmac_desc::FS;
	size_t b = block_count;
	int index = 0;
	for (index = 0; b; index++, phys_addr += 0x1000, flags = Idmac_desc::NONE) {
		b = _idmac_desc[index].set(b, block_size(), phys_addr, flags);
		_idmac_desc[index].next =
			_idmac_desc_phys + ((index + 1) * sizeof(Idmac_desc));
	}
	_idmac_desc[index].next = (unsigned)_idmac_desc;
	_idmac_desc[index].flags |= Idmac_desc::ER;
	Mmio::write<Dbaddr>(_idmac_desc_phys);

	Mmio::write<Ctrl::Dma_enable>(1);
	Mmio::write<Ctrl::Use_internal_dmac>(1);

	Mmio::write<Bmod::Fixed_burst>(1);
	Mmio::write<Bmod::Idmac_enable>(1);

	Mmio::write<Blksize>(block_size());
	Mmio::write<Bytcnt>(block_size() * block_count);

	Mmio::write<Pldmnd>(1);

	return true;
}


void Driver::_handle_irq()
{
	_irq.ack_irq();

	if (!_block_transfer.pending) {
		return; }

	bool success = false;
	if (Mmio::read<Rintsts::Response_error>()) {
		error("Response error");
	}
	if (Mmio::read<Rintsts::Data_read_timeout>()) {
		error("Data read timeout");
	}
	if (Mmio::read<Rintsts::Data_crc_error>()) {
		error("CRC error");
	}
	if (Mmio::read<Rintsts::Data_transfer_over>()) {
		Mmio::write<Rintsts>(~0U);
		if (!_issue_command(Stop_transmission())) {
			error("unable to stop transmission");
		} else {
			success = true;
		}
	}
	_block_transfer.pending = false;
	ack_packet(_block_transfer.packet, success);
}


bool Driver::_issue_command(Command_base const &command)
{
	try { wait_for(Attempts(10000), Microseconds(100), _delayer,
	               Status::Data_busy::Equal(0)); }
	catch (Polling_timeout) {
		error("wait for State::Data_busy timed out ",
		      Hex(Mmio::read<Status>()));
		return false;
	}

	Mmio::write<Rintsts>(~0UL);

	/* write command argument */
	Mmio::write<Cmdarg>(command.arg);

	Cmd::access_t cmd = 0;
	Cmd::Index::set(cmd, command.index);

	if (command.transfer != TRANSFER_NONE) {
		/* set data-direction bit depending on the command */
		bool const write = command.transfer == TRANSFER_WRITE;
		Cmd::Data_expected::set(cmd, 1);
		Cmd::Write::set(cmd, write ? 1 : 0);
	}

	Cmd::access_t rsp_type = 0;
	switch (command.rsp_type) {
	case RESPONSE_NONE:             rsp_type = Cmd::Rsp_type::RESPONSE_NONE;             break;
	case RESPONSE_136_BIT:          rsp_type = Cmd::Rsp_type::RESPONSE_136_BIT;          break;
	case RESPONSE_48_BIT:           rsp_type = Cmd::Rsp_type::RESPONSE_48_BIT;           break;
	case RESPONSE_48_BIT_WITH_BUSY: rsp_type = Cmd::Rsp_type::RESPONSE_48_BIT_WITH_BUSY; break;
	}

	Cmd::Rsp_type::set(cmd, rsp_type);
	Cmd::Start_cmd::set(cmd, 1);
	Cmd::Use_hold_reg::set(cmd ,1);
	Cmd::Wait_prvdata_complete::set(cmd, 1);

	if (command.index == 0)
		Cmd::Init_sequence::set(cmd, 1);

	/* issue command */
	Mmio::write<Cmd>(cmd);

	try { wait_for(Attempts(10000), Microseconds(100), _delayer,
	               Rintsts::Command_done::Equal(1)); }
	catch (Polling_timeout) {
		error("command failed "
		      "Rintst: ", Mmio::read<Rintsts>(), " "
		      "Mintst: ", Mmio::read<Mintsts>(), " "
		      "Status: ", Mmio::read<Status>());

		if (Mmio::read<Rintsts::Response_timeout>())
			warning("timeout");

		if (Mmio::read<Rintsts::Response_error>())
			warning("repsonse error");

		return false;
	}

	/* acknowledge interrupt */
	Mmio::write<Rintsts::Command_done>(1);

	_delayer.usleep(100);

	return true;
}


Cid Driver::_read_cid()
{
	Cid cid;
	cid.raw_0 = Mmio::read<Rsp0>();
	cid.raw_1 = Mmio::read<Rsp1>();
	cid.raw_2 = Mmio::read<Rsp2>();
	cid.raw_3 = Mmio::read<Rsp3>();
	return cid;
}


Csd Driver::_read_csd()
{
	Csd csd;
	csd.csd0 = Mmio::read<Rsp0>();
	csd.csd1 = Mmio::read<Rsp1>();
	csd.csd2 = Mmio::read<Rsp2>();
	csd.csd3 = Mmio::read<Rsp3>();
	return csd;
}


size_t Driver::_read_ext_csd()
{
	Attached_ram_dataspace ds(&_env.ram(), 0x1000, UNCACHED);

	addr_t phys = Dataspace_client(ds.cap()).phys_addr();
	_setup_idmac_descriptor_table(1, phys);

	if (!issue_command(Mmc_send_ext_csd()))
		throw Detection_failed();

	try { wait_for(_delayer, Rintsts::Data_transfer_over::Equal(1)); }
	catch (Polling_timeout) {
		error("cannot retrieve extented CSD");
		throw Detection_failed();
	}
	/* clear IRQ */
	Mmio::write<Rintsts::Data_transfer_over>(1);

	/* contruct extented CSD */
	Ext_csd csd((addr_t)ds.local_addr<addr_t>());

	/* read revision */
	if (csd.Mmio::read<Ext_csd::Revision>() < 2) {
		error("extented CSD revision is < 2");
		throw Detection_failed();
	}

	/* return sector count */
	uint64_t capacity =  csd.Mmio::read<Ext_csd::Sector_count>() * block_size();

	/* to MB */
	return capacity / (1024 * 1024);
}


size_t Driver::Idmac_desc::set(size_t block_count,
                               size_t block_size,
                               addr_t phys_addr,
                               Flags  flag)
{
	enum  { MAX_BLOCKS = 8 };
	flags = OWN | flag |
	        (block_count <= MAX_BLOCKS ? LD : (CH | DIC));
	bytes = ((block_count < MAX_BLOCKS) ? block_count : MAX_BLOCKS) *
	        block_size;
	addr  = phys_addr;

	return block_count < MAX_BLOCKS ?
	       0 : block_count  - MAX_BLOCKS;
}
