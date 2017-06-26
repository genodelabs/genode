/*
 * \brief  Raspberry Pi SDHCI driver
 * \author Norman Feske
 * \author Christian Helmuth
 * \author Timo Wischer
 * \author Martin Stein
 * \date   2014-09-21
 */

/*
 * Copyright (C) 2014-2017 Genode Labs GmbH
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
	Attached_mmio(env, Rpi::SDHCI_BASE, Rpi::SDHCI_SIZE),
	_env(env)
{
	log("SD card detected");
	log("capacity: ", _card_info.capacity_mb(), " MiB");
}


void Driver::_set_and_enable_clock(unsigned divider)
{
	Control1::access_t ctl1 = Mmio::Mmio::read<Control1>();
	Control1::Clk_freq8::set(ctl1, divider);
	Control1::Clk_freq_ms2::set(ctl1, 0);
	Control1::Clk_internal_en::set(ctl1, 1);
	Mmio::write<Control1>(ctl1);

	try {
		wait_for(_delayer, Control1::Clk_internal_stable::Equal(1));
	} catch (Polling_timeout) {
		error("could not set internal clock");
		throw Detection_failed();
	}
	Mmio::write<Control1::Clk_en>(1);
	_delayer.usleep(10*1000);

	/* data timeout unit exponent */
	Mmio::write<Control1::Data_tounit>(0xe);
}


Card_info Driver::_init()
{
	/* reset host controller */
	Control1::access_t v = Mmio::read<Control1>();
	Control1::Srst_hc::set(v);
	Control1::Srst_data::set(v);
	Mmio::write<Control1>(v);

	try {
		wait_for(_delayer, Control1::Srst_hc::Equal(0));
	} catch (Polling_timeout) {
		error("host-controller soft reset timed out");
		throw Detection_failed();
	}
	log("SDHCI version: ", Mmio::read<Host_version::Vendor>(), " "
	    "(specification ", Mmio::read<Host_version::Spec>() + 1, ".0)");

	/* Enable sd card power */
	Mmio::write<Host_ctrl>(Host_ctrl::Power::bits(1)
	               | Host_ctrl::Voltage::bits(Host_ctrl::Voltage::V33));

	/* enable interrupt status reporting */
	Mmio::write<Irpt_mask>(~0UL);
	Mmio::write<Irpt_en>(~0UL);

	/*
	 * We don't read the capability register as the BCM2835 always
	 * returns all bits set to zero.
	 */

	_set_and_enable_clock(240);

	if (!issue_command(Go_idle_state())) {
		warning("Go_idle_state command failed");
		throw Detection_failed();
	}
	_delayer.usleep(2000);

	if (!issue_command(Send_if_cond())) {
		warning("Send_if_cond command failed");
		throw Detection_failed();
	}
	if (Mmio::read<Resp0>() != 0x1aa) {
		error("unexpected response of Send_if_cond command");
		throw Detection_failed();
	}

	/*
	 * We need to issue the same Sd_send_op_cond command multiple
	 * times. The first time, we receive the status information. On
	 * subsequent attempts, the response tells us that the card is
	 * busy. Usually, the command is issued twice. We give up if the
	 * card is not reaching busy state after one second.
	 */

	int i = 1000;
	for (; i > 0; --i) {
		if (!issue_command(Sd_send_op_cond(0x18000, true))) {
			warning("Sd_send_op_cond command failed");
			throw Detection_failed();
		}
		if (Ocr::Busy::get(Mmio::read<Resp0>()))
			break;

		_delayer.usleep(1000);
	}
	if (i == 0) {
		error("Sd_send_op_cond timed out, could no power-on SD card");
		throw Detection_failed();
	}
	Card_info card_info = _detect();

	/*
	 * Switch card to use 4 data signals
	 */
	if (!issue_command(Set_bus_width(Set_bus_width::Arg::Bus_width::FOUR_BITS),
	                   card_info.rca())) {
		warning("Set_bus_width(FOUR_BITS) command failed");
		throw Detection_failed();
	}
	/* switch host controller to use 4 data signals */
	Control0::access_t ctl0 = Mmio::read<Control0>();
	Control0::Hctl_dwidth::set(ctl0);
	Control0::Hctl_hs_en::set(ctl0);
	Mmio::write<Control0>(ctl0);
	_delayer.usleep(10*1000);

	/*
	 * Accelerate clock, the divider is hard-coded for now.
	 *
	 * The Raspberry Pi report as clock of 250 MHz. According to the
	 * SDHCI specification, it is possible to driver SD cards with
	 * 50 MHz in high-speed mode (Hctl_hs_en).
	 */
	_set_and_enable_clock(5);
	return card_info;
}


void Driver::_set_block_count(size_t block_count)
{
	/*
	 * The 'Blksizecnt' register must be written in one step. If we
	 * used subsequent writes for the 'Blkcnt' and 'Blksize' bitfields,
	 * the host controller of the BCM2835 would fail to recognize any
	 * but the first write operation.
	 */
	Blksizecnt::access_t v = Mmio::read<Blksizecnt>();
	Blksizecnt::Blkcnt::set(v, block_count);
	Blksizecnt::Blksize::set(v, block_size());
	Mmio::write<Blksizecnt>(v);
}


size_t Driver::_block_to_command_address(const size_t block_number)
{
	/* use byte position for addressing with standard cards */
	if (_card_info.version() == Csd3::Version::STANDARD_CAPACITY) {
		return block_number * block_size();
	}
	return block_number;
}


bool Driver::_issue_command(Command_base const &command)
{
	if (!_poll_and_wait_for<Status::Inhibit>(0)) {
		error("controller inhibits issueing commands");
		return false;
	}
	/* write command argument */
	Mmio::write<Arg1>(command.arg);

	/* assemble command register */
	Cmdtm::access_t cmd = 0;
	Cmdtm::Index::set(cmd, command.index);
	if (command.transfer != TRANSFER_NONE) {

		Cmdtm::Isdata::set(cmd);
		Cmdtm::Tm_blkcnt_en::set(cmd);
		Cmdtm::Tm_multi_block::set(cmd);

		if (command.index == Read_multiple_block::INDEX ||
		    command.index == Write_multiple_block::INDEX)
		{
			Cmdtm::Tm_auto_cmd_en::set(cmd, Cmdtm::Tm_auto_cmd_en::CMD12);
		}
		/* set data-direction bit depending on the command */
		bool const read = command.transfer == TRANSFER_READ;
		Cmdtm::Tm_dat_dir::set(cmd, read ? Cmdtm::Tm_dat_dir::READ
		                                 : Cmdtm::Tm_dat_dir::WRITE);
	}
	Cmdtm::access_t rsp_type = 0;
	switch (command.rsp_type) {
	case RESPONSE_NONE:             rsp_type = Cmdtm::Rsp_type::RESPONSE_NONE;             break;
	case RESPONSE_136_BIT:          rsp_type = Cmdtm::Rsp_type::RESPONSE_136_BIT;          break;
	case RESPONSE_48_BIT:           rsp_type = Cmdtm::Rsp_type::RESPONSE_48_BIT;           break;
	case RESPONSE_48_BIT_WITH_BUSY: rsp_type = Cmdtm::Rsp_type::RESPONSE_48_BIT_WITH_BUSY; break;
	}
	Cmdtm::Rsp_type::set(cmd, rsp_type);

	/* write command */
	Mmio::write<Cmdtm>(cmd);

	if (!_poll_and_wait_for<Interrupt::Cmd_done>(1)) {
		error("command timed out");
		return false;
	}
	/* clear interrupt state */
	Mmio::write<Interrupt::Cmd_done>(1);

	return true;
}


Cid Driver::_read_cid()
{
	Cid cid;
	cid.raw_0 = Mmio::read<Resp0_136>();
	cid.raw_1 = Mmio::read<Resp1_136>();
	cid.raw_2 = Mmio::read<Resp2_136>();
	cid.raw_3 = Mmio::read<Resp3_136>();
	return cid;
}


Csd Driver::_read_csd()
{
	Csd csd;
	csd.csd0 = Mmio::read<Resp0_136>();
	csd.csd1 = Mmio::read<Resp1_136>();
	csd.csd2 = Mmio::read<Resp2_136>();
	csd.csd3 = Mmio::read<Resp3_136>();
	return csd;
}


void Driver::read(Block::sector_t           block_number,
                  size_t                    block_count,
                  char                     *out_buffer,
                  Block::Packet_descriptor &packet)
{
	_set_block_count(block_count);

	if (!issue_command(Read_multiple_block(_block_to_command_address(block_number)))) {
		error("Read_multiple_block failed");
		throw Io_error();
	}
	Data::access_t *dst = (Data::access_t *)(out_buffer);
	for (size_t i = 0; i < block_count; i++) {

		/*
		 * Check for buffer-read enable bit for each block
		 *
		 * According to the BCM2835 documentation, this bit is
		 * reserved but it actually corresponds to the bre status
		 * bit as described in the SDHCI specification.
		 */
		if (!_poll_and_wait_for<Status::Bre>(1)) {
			throw Io_error(); }

		/* read data from sdhci buffer */
		for (size_t j = 0; j < block_size() / sizeof(Data::access_t); j++)
			*dst++ = Mmio::read<Data>();
	}
	if (!_poll_and_wait_for<Interrupt::Data_done>(1)) {
		error("completion of read request failed");
		throw Io_error();
	}
	/* clear interrupt state */
	Mmio::write<Interrupt::Data_done>(1);

	ack_packet(packet);
}


void Driver::write(Block::sector_t           block_number,
                   size_t                    block_count,
                   char const               *buffer,
                   Block::Packet_descriptor &packet)
{
	_set_block_count(block_count);

	if (!issue_command(Write_multiple_block(_block_to_command_address(block_number)))) {
		error("Write_multiple_block failed");
		throw Io_error();
	}
	Data::access_t const *src = (Data::access_t const *)(buffer);
	for (size_t i = 0; i < block_count; i++) {

		/* check for buffer-write enable bit for each block */
		if (!_poll_and_wait_for<Status::Bwe>(1)) {
			throw Io_error(); }

		/* write data into sdhci buffer */
		for (size_t j = 0; j < block_size() / sizeof(Data::access_t); j++)
			Mmio::write<Data>(*src++);
	}
	if (!_poll_and_wait_for<Interrupt::Data_done>(1)) {
		error("completion of write request failed");
		throw Io_error();
	}
	/* clear interrupt state */
	Mmio::write<Interrupt::Data_done>(1);

	ack_packet(packet);
}
