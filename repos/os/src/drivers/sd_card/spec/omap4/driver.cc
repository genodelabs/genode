/*
 * \brief  OMAP4-specific implementation of the Block::Driver interface
 * \author Norman Feske
 * \date   2012-07-19
 */

/*
 * Copyright (C) 2012-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* local includes */
#include <driver.h>

using namespace Genode;
using namespace Sd_card;


Card_info Driver::_init()
{
	Mmio::write<Sysconfig>(0x2015);
	Mmio::write<Hctl>(0x0);

	_set_bus_power(VOLTAGE_3_0);

	if (!_sd_bus_power_on()) {
		error("sd_bus_power failed");
	}
	_disable_irq();

	_bus_width(BUS_WIDTH_1);

	_delayer.usleep(10*1000);

	_stop_clock();
	if (!_set_and_enable_clock(CLOCK_DIV_240)) {
		error("set_clock failed");
		throw Detection_failed();
	}
	if (!_init_stream()) {
		error("sending the initialization stream failed");
		throw Detection_failed();
	}
	Mmio::write<Blk>(0);
	_delayer.usleep(1000);

	if (!issue_command(Go_idle_state())) {
		error("Go_idle_state command failed");
		throw Detection_failed();
	}
	_delayer.usleep(2000);

	if (!issue_command(Send_if_cond())) {
		error("Send_if_cond command failed");
		throw Detection_failed();
	}
	if (Mmio::read<Rsp10>() != 0x1aa) {
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

		if (Ocr::Busy::get(Mmio::read<Rsp10>()))
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
	_bus_width(BUS_WIDTH_4);
	_delayer.usleep(10*1000);

	_stop_clock();
	if (!_set_and_enable_clock(CLOCK_DIV_0)) {
		error("set_clock failed");
		throw Detection_failed();
	}

	/* enable IRQs */
	Mmio::write<Ie::Tc_enable>(1);
	Mmio::write<Ie::Cto_enable>(1);
	Mmio::write<Ise::Tc_sigen>(1);
	Mmio::write<Ise::Cto_sigen>(1);

	return card_info;
}


bool Driver::_wait_for_bre()
{
	try { wait_for(Attempts(1000000), Microseconds(0), _delayer,
	               Pstate::Bre::Equal(1)); }
	catch (Polling_timeout) {
		try { wait_for(_delayer, Pstate::Bre::Equal(1)); }
		catch (Polling_timeout) {
			error("Pstate::Bre timed out");
			return false;
		}
	}
	return true;
}


bool Driver::_wait_for_bwe()
{
	try { wait_for(Attempts(1000000), Microseconds(0), _delayer,
	               Pstate::Bwe::Equal(1)); }
	catch (Polling_timeout) {
		try { wait_for(_delayer, Pstate::Bwe::Equal(1)); }
		catch (Polling_timeout) {
			error("Pstate::Bwe timed out");
			return false;
		}
	}
	return true;
}


void Driver::_handle_irq()
{
	_irq.ack_irq();

	if (!_block_transfer.pending) {
		return; }

	if (Mmio::read<Stat::Tc>() != 1) {
		warning("unexpected interrupt, Stat: ", Hex(Mmio::read<Stat>()));
		return;
	}

	Mmio::write<Stat::Tc>(1);

	if (Mmio::read<Stat>() != 0) {
		warning("unexpected state ("
		                "Stat: ", Hex(Mmio::read<Stat>()), " "
		                "Blen: ", Hex(Mmio::read<Blk::Blen>()), " "
		                "Nblk: ", Mmio::read<Blk::Nblk>());
		return;
	}
	_block_transfer.pending = false;
	ack_packet(_block_transfer.packet, true);
}


bool Driver::_reset_cmd_line()
{
	Mmio::write<Sysctl::Src>(1);

	/*
	 * We must poll quickly. If we waited too long until checking the
	 * bit, the polling would be infinite. Apparently the hardware
	 * depends on the timing here.
	 */
	try { wait_for(Attempts(1000), Microseconds(0), _delayer,
	               Sysctl::Src::Equal(1)); }
	catch (Polling_timeout) {
		error("reset of cmd line timed out (src != 1)");
		return false;
	}
	try { wait_for(Attempts(1000), Microseconds(0), _delayer,
	               Sysctl::Src::Equal(0)); }
	catch (Polling_timeout) {
		error("reset of cmd line timed out (src != 0)");
		return false;
	}
	return true;
}


void Driver::_disable_irq()
{
	Mmio::write<Ise>(0);
	Mmio::write<Ie>(0);
	Mmio::write<Stat>(~0);
}


void Driver::_bus_width(Bus_width bus_width)
{
	switch (bus_width) {
	case BUS_WIDTH_1:
		Mmio::write<Con::Dw8>(0);
		Mmio::write<Hctl::Dtw>(Hctl::Dtw::ONE_BIT);
		break;

	case BUS_WIDTH_4:
		Mmio::write<Con::Dw8>(0);
		Mmio::write<Hctl::Dtw>(Hctl::Dtw::FOUR_BITS);
		break;
	}
}


bool Driver::_sd_bus_power_on()
{
	Mmio::write<Hctl::Sdbp>(Hctl::Sdbp::POWER_ON);

	try { wait_for(_delayer, Hctl::Sdbp::Equal(1)); }
	catch (Polling_timeout) {
		error("setting Hctl::Sdbp timed out");
		return false;
	}
	return true;
}


bool Driver::_set_and_enable_clock(enum Clock_divider divider)
{
	Mmio::write<Sysctl::Dto>(Sysctl::Dto::TCF_2_POW_27);

	switch (divider) {
	case CLOCK_DIV_0:   Mmio::write<Sysctl::Clkd>(0);   break;
	case CLOCK_DIV_240: Mmio::write<Sysctl::Clkd>(240); break;
	}

	Mmio::write<Sysctl::Ice>(1);

	/* wait for clock to become stable */
	try { wait_for(_delayer, Sysctl::Ics::Equal(1)); }
	catch (Polling_timeout) {
		error("clock enable timed out");
		return false;
	}
	/* enable clock */
	Mmio::write<Sysctl::Ce>(1);

	return true;
}


void Driver::_set_bus_power(Voltage voltage)
{
	switch (voltage) {
	case VOLTAGE_3_0:
		Mmio::write<Hctl::Sdvs>(Hctl::Sdvs::VOLTAGE_3_0);
		break;
	case VOLTAGE_1_8:
		Mmio::write<Hctl::Sdvs>(Hctl::Sdvs::VOLTAGE_1_8);
		break;
	}
	Mmio::write<Capa::Vs18>(1);

	if (voltage == VOLTAGE_3_0)
		Mmio::write<Capa::Vs30>(1);
}


bool Driver::_init_stream()
{
	Mmio::write<Ie>(0x307f0033);

	/* start initialization sequence */
	Mmio::write<Con::Init>(1);
	Mmio::write<Cmd>(0);

	try { wait_for(Attempts(1000000), Microseconds(0), _delayer,
	               Stat::Cc::Equal(1)); }
	catch (Polling_timeout) {
		error("init stream timed out");
		return false;
	}
	/* stop initialization sequence */
	Mmio::write<Con::Init>(0);
	Mmio::write<Stat>(~0);
	Mmio::read<Stat>();
	return true;
}


bool Driver::_issue_command(Command_base const &command)
{
	try { wait_for(_delayer, Pstate::Cmdi::Equal(0)); }
	catch (Polling_timeout) {
		error("wait for Pstate::Cmdi timed out");
		return false;
	}
	/* write command argument */
	Mmio::write<Arg>(command.arg);

	/* assemble command register */
	Cmd::access_t cmd = 0;
	Cmd::Index::set(cmd, command.index);
	if (command.transfer != TRANSFER_NONE) {

		Cmd::Dp::set(cmd);
		Cmd::Bce::set(cmd);
		Cmd::Msbs::set(cmd);

		if (command.index == Read_multiple_block::INDEX ||
		    command.index == Write_multiple_block::INDEX)
		{
			Cmd::Acen::set(cmd);
		}
		/* set data-direction bit depending on the command */
		bool const read = command.transfer == TRANSFER_READ;
		Cmd::Ddir::set(cmd, read ? Cmd::Ddir::READ : Cmd::Ddir::WRITE);
	}
	Cmd::access_t rsp_type = 0;
	switch (command.rsp_type) {
	case RESPONSE_NONE:             rsp_type = Cmd::Rsp_type::RESPONSE_NONE;             break;
	case RESPONSE_136_BIT:          rsp_type = Cmd::Rsp_type::RESPONSE_136_BIT;          break;
	case RESPONSE_48_BIT:           rsp_type = Cmd::Rsp_type::RESPONSE_48_BIT;           break;
	case RESPONSE_48_BIT_WITH_BUSY: rsp_type = Cmd::Rsp_type::RESPONSE_48_BIT_WITH_BUSY; break;
	}
	Cmd::Rsp_type::set(cmd, rsp_type);

	/* write command */
	Mmio::write<Cmd>(cmd);
	bool result = false;

	/* wait until command is completed, return false on timeout */
	for (unsigned long i = 0; i < 1000*1000; i++) {
		Stat::access_t const stat = Mmio::read<Stat>();
		if (Stat::Erri::get(stat)) {
			warning("SD command error");
			if (Stat::Cto::get(stat))
				warning("timeout");

			_reset_cmd_line();
			Mmio::write<Stat::Cc>(~0);
			Mmio::read<Stat>();
			result = false;
			break;
		}
		if (Stat::Cc::get(stat) == 1) {
			result = true;
			break;
		}
	}
	/* clear status of command-completed bit */
	Mmio::write<Stat::Cc>(1);
	Mmio::read<Stat>();
	return result;
}


Cid Driver::_read_cid()
{
	Cid cid;
	cid.raw_0 = Mmio::read<Rsp10>();
	cid.raw_1 = Mmio::read<Rsp32>();
	cid.raw_2 = Mmio::read<Rsp54>();
	cid.raw_3 = Mmio::read<Rsp76>();
	return cid;
}


Csd Driver::_read_csd()
{
	Csd csd;
	csd.csd0 = Mmio::read<Rsp10>();
	csd.csd1 = Mmio::read<Rsp32>();
	csd.csd2 = Mmio::read<Rsp54>();
	csd.csd3 = Mmio::read<Rsp76>();
	return csd;
}


Driver::Driver(Env &env)
:
	Driver_base(env.ram()),
	Attached_mmio(env, MMCHS1_MMIO_BASE, MMCHS1_MMIO_SIZE), _env(env)
{
	_irq.sigh(_irq_handler);
	_irq.ack_irq();
	log("SD card detected");
	log("capacity: ", _card_info.capacity_mb(), " MiB");
}


void Driver::read(Block::sector_t           block_number,
                  size_t                    block_count,
                  char                     *buffer,
                  Block::Packet_descriptor &pkt)
{
	if (_block_transfer.pending) {
		throw Request_congestion(); }

	Mmio::write<Blk::Blen>(block_size());
	Mmio::write<Blk::Nblk>(block_count);

	_block_transfer.packet  = pkt;
	_block_transfer.pending = true;

	if (!issue_command(Read_multiple_block(block_number))) {
		error("Read_multiple_block failed");
		throw Io_error();
	}
	size_t const num_accesses = block_count * block_size() /
	                            sizeof(Data::access_t);
	Data::access_t *dst = (Data::access_t *)(buffer);

	for (size_t i = 0; i < num_accesses; i++) {
		if (!_wait_for_bre())
			throw Io_error();

		*dst++ = Mmio::read<Data>();
	}
}


void Driver::write(Block::sector_t           block_number,
                   size_t                    block_count,
                   char const               *buffer,
                   Block::Packet_descriptor &pkt)
{
	if (_block_transfer.pending) {
		throw Request_congestion(); }

	Mmio::write<Blk::Blen>(block_size());
	Mmio::write<Blk::Nblk>(block_count);

	_block_transfer.packet  = pkt;
	_block_transfer.pending = true;

	if (!issue_command(Write_multiple_block(block_number))) {
		error("Write_multiple_block failed");
		throw Io_error();
	}
	size_t const num_accesses = block_count * block_size() /
	                            sizeof(Data::access_t);
	Data::access_t const *src = (Data::access_t const *)(buffer);

	for (size_t i = 0; i < num_accesses; i++) {
		if (!_wait_for_bwe()) {
			throw Io_error(); }

		Mmio::write<Data>(*src++);
	}
}
