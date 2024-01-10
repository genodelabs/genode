/*
 * \brief  Secured Digital Host Controller
 * \author Martin Stein
 * \date   2015-02-05
 */

/*
 * Copyright (C) 2015-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* local includes */
#include <driver.h>

using namespace Sd_card;
using namespace Genode;


int Driver::_wait_for_card_ready_mbw()
{
	/*
	 * Poll card status
	 *
	 * The maximum number of attempts and the delay between two attempts are
	 * freely chosen.
	 */
	unsigned           attempts          = 5;
	uint64_t constexpr attempts_delay_us = 100000;
	while (1) {
		if (!attempts) {
			error("Reading card status after multiblock write failed");
			return -1;
		}
		/* assemble argument register value */
		Send_status::Arg::access_t cmdarg = 0;
		Send_status::Arg::Rca::set(cmdarg, _card_info.rca());

		/* assemble command register value */
		Xfertyp::access_t xfertyp = 0;
		Xfertyp::Cmdinx::set(xfertyp, Send_status::INDEX);
		Xfertyp::Cicen::set(xfertyp, 1);
		Xfertyp::Cccen::set(xfertyp, 1);
		Xfertyp::Rsptyp::set(xfertyp, Xfertyp::Rsptyp::_48BIT);
		Xfertyp::Msbsel::set(xfertyp,  1);
		Xfertyp::Bcen::set(xfertyp, 1);
		Xfertyp::Dmaen::set(xfertyp, 1);

		/* send command as soon as the host allows it */
		if (_wait_for_cmd_allowed()) { return -1; }
		Mmio::write<Cmdarg>(cmdarg);
		Mmio::write<Xfertyp>(xfertyp);

		/* wait for command completion */
		if (_wait_for_cmd_complete()) { return -1; }

		/* check for errors */
		R1_response_0::access_t const resp = Mmio::read<Cmdrsp0>();
		if (R1_response_0::Error::get(resp)) {
			error("Reading card status after multiblock write failed");
			return -1;
		}
		/* if card is in a ready state, return success, retry otherwise */
		if (R1_response_0::card_ready(resp)) { break; }
		_delayer.usleep(attempts_delay_us);
	}
	return 0;
}


int Driver::_stop_transmission()
{
	/* write argument register */
	Mmio::write<Cmdarg>(0);

	/* write command register */
	Xfertyp::access_t xfertyp = 0;
	Xfertyp::Cmdinx::set(xfertyp, Stop_transmission::INDEX);
	Xfertyp::Cmdtyp::set(xfertyp, Xfertyp::Cmdtyp::ABORT_CMD12);
	Xfertyp::Cccen::set(xfertyp, 1);
	Xfertyp::Cicen::set(xfertyp, 1);
	Xfertyp::Rsptyp::set(xfertyp, Xfertyp::Rsptyp::_48BIT_BUSY);
	_stop_transmission_finish_xfertyp(xfertyp);
	Mmio::write<Xfertyp>(xfertyp);

	/* wait for command completion */
	if (_wait_for_cmd_complete()) { return -1; }
	return 0;
}


void Driver::_handle_irq()
{
	_irq.ack();

	/* the handler is only for block transfers, on other commands we poll */
	if (!_block_transfer.pending) {
		return; }

	/*
	 * The host signals on multi-block transfers seem to be broken.
	 * Synchronizing to "Transfer Complete" before returning from transfers
	 * and to "Command Inhibit" before sending further commands - as it is
	 * done with other controllers - isn't sufficient. Instead, both "Transfer
	 * Complete" and "Command Complete" must be gathered.
	 */
	try {
		wait_for(Attempts(1000), Microseconds(1000), _delayer,
		         Irqstat::Cc::Equal(1), Irqstat::Tc::Equal(1)); }
	catch (Polling_timeout) {
		error("Completion host signal timed out");
		throw -1;
	}
	/* acknowledge completion signals */
	Irqstat::access_t irqstat = 0;
	Irqstat::Cc::set(irqstat, 1);
	Irqstat::Tc::set(irqstat, 1);
	Mmio::write<Irqstat>(irqstat);

	if (_wait_for_cmd_complete_mb_finish(_block_transfer.read)) {
		throw -1; }

	_block_transfer.pending = false;
	ack_packet(_block_transfer.packet, true);
}


int Driver::_wait_for_cmd_complete()
{
	try { wait_for(Attempts(200), Microseconds(5000), _delayer,
	               Irqstat::Cc::Equal(1)); }
	catch (Polling_timeout) {
		error("command timed out");
		return -1;
	}
	Mmio::write<Irqstat>(Irqstat::Cc::reg_mask());
	return 0;
}


bool Driver::_issue_command(Command_base const & command)
{
	/* get command characteristics */
	bool const transfer   = command.transfer != TRANSFER_NONE;
	bool const reading    = command.transfer == TRANSFER_READ;
	bool const multiblock =
		command.index == Read_multiple_block::INDEX ||
		command.index == Write_multiple_block::INDEX;

	/* set command index */
	Xfertyp::access_t xfertyp = 0;
	Xfertyp::Cmdinx::set(xfertyp, command.index);

	/* select response type */
	typedef Xfertyp::Rsptyp Rsptyp;
	Xfertyp::access_t rsptyp = 0;
	switch (command.rsp_type) {
	case RESPONSE_NONE:             rsptyp = Rsptyp::_0BIT;       break;
	case RESPONSE_136_BIT:          rsptyp = Rsptyp::_136BIT;     break;
	case RESPONSE_48_BIT:           rsptyp = Rsptyp::_48BIT;      break;
	case RESPONSE_48_BIT_WITH_BUSY: rsptyp = Rsptyp::_48BIT_BUSY; break;
	}
	Xfertyp::Rsptyp::set(xfertyp, rsptyp);

	/* generic transfer settings */
	if (command.transfer != TRANSFER_NONE) {
		Xfertyp::Dpsel::set(xfertyp);
		if (multiblock) {
			Xfertyp::Cicen::set(xfertyp, 1);
			Xfertyp::Cccen::set(xfertyp, 1);
		}
	}
	/* version-dependent transfer settings and issue command */
	_issue_cmd_finish_xfertyp(xfertyp, transfer, multiblock, reading);
	Mmio::write<Cmdarg>(command.arg);
	Mmio::write<Xfertyp>(xfertyp);

	/* for block transfers there's a signal handler, on other commands poll */
	return transfer ? true : !_wait_for_cmd_complete();
}


Cid Driver::_read_cid()
{
	Cid cid;
	cid.raw_0 = Mmio::read<Rsp136_0>();
	cid.raw_1 = Mmio::read<Rsp136_1>();
	cid.raw_2 = Mmio::read<Rsp136_2>();
	cid.raw_3 = Mmio::read<Rsp136_3>();
	return cid;
}


Csd Driver::_read_csd()
{
	Csd csd;
	csd.csd0 = Mmio::read<Rsp136_0>();
	csd.csd1 = Mmio::read<Rsp136_1>();
	csd.csd2 = Mmio::read<Rsp136_2>();
	csd.csd3 = Mmio::read<Rsp136_3>();
	return csd;
}


unsigned Driver::_read_rca()
{
	Cmdrsp0::access_t const rsp0 = Mmio::read<Cmdrsp0>();
	return Send_relative_addr::Response::Rca::get(rsp0);
}


void Driver::read_dma(Block::sector_t           blk_nr,
                      size_t                    blk_cnt,
                      addr_t                    buf_phys,
                      Block::Packet_descriptor &packet)
{
	if (_prepare_dma_mb(packet, true, blk_cnt, buf_phys)) {
		throw Io_error(); }

	if (!issue_command(Read_multiple_block((unsigned long)blk_nr))) {
		throw Io_error(); }
}


void Driver::write_dma(Block::sector_t           blk_nr,
                       size_t                    blk_cnt,
                       addr_t                    buf_phys,
                       Block::Packet_descriptor &packet)
{
	if (_prepare_dma_mb(packet, false, blk_cnt, buf_phys)) {
		throw Io_error(); }

	if (!issue_command(Write_multiple_block((unsigned long)blk_nr))) {
		throw Io_error(); }
}


int Driver::_prepare_dma_mb(Block::Packet_descriptor packet,
                          bool                     reading,
                          size_t                   blk_cnt,
                          addr_t                   buf_phys)
{
	if (_block_transfer.pending) {
		throw Request_congestion(); }


	/* write ADMA2 table to DMA */
	size_t const req_size = blk_cnt * _block_size();
	if (_adma2_table.setup_request(req_size, buf_phys)) { return -1; }

	/* configure DMA at host */
	Mmio::write<Adsaddr>(_adma2_table.base_dma());
	Mmio::write<Blkattr::Blksize>(_block_size());
	Mmio::write<Blkattr::Blkcnt>(blk_cnt);

	_block_transfer.read    = reading;
	_block_transfer.packet  = packet;
	_block_transfer.pending = true;
	return 0;
}


int Driver::_wait_for_cmd_allowed()
{
	/*
	 * At least after multi-block writes on i.MX53 with the fix for the broken
	 * "Auto Command 12", waiting only for "Command Inhibit" isn't sufficient
	 * as "Data Line Active" and "Data Inhibit" may also be active.
	 */
	try { wait_for(_delayer, Prsstat::Dla::Equal(0),
	                         Prsstat::Sdstb::Equal(1),
	                         Prsstat::Cihb::Equal(0),
	                         Prsstat::Cdihb::Equal(0)); }
	catch (Polling_timeout) {
		error("wait till issuing a new command is allowed timed out");
		return -1;
	}
	return 0;
}


Card_info Driver::_init()
{
	/* install IRQ signal */
	_irq.sigh(_irq_handler);

	/* configure host for initialization stage */
	if (_reset()) {
		_detect_err("Host reset failed"); }
	_disable_irqs();

	if (!_supported_host_version(Mmio::read<Hostver>())) {
		error("host version not supported");
		throw Detection_failed();
	}

	/*
	 * We should check host capabilities at this point if we want to
	 * support other versions of the SDHC. For the already supported
	 * versions we know that the capabilities fit our requirements.
	 */

	/* configure IRQs, bus width, and clock for initialization */
	_enable_irqs();
	_bus_width(BUS_WIDTH_1);
	_delayer.usleep(10000);
	_clock(CLOCK_INITIAL);

	/*
	 * Initialize card
	 */

	/*
	 * At this point we should do an SDIO card reset if we later want
	 * to detect the unwanted case of an SDIO card beeing inserted.
	 * The reset would be done via 2 differently configured
	 * Io_rw_direct commands.
	 */

	_delayer.usleep(1000);
	if (!issue_command(Go_idle_state())) {
		_detect_err("Go_idle_state command failed"); }

	_delayer.usleep(2000);
	if (!issue_command(Send_if_cond())) {
		_detect_err("Send_if_cond command failed"); }

	if (Mmio::read<Cmdrsp0>() != 0x1aa) {
		_detect_err("Unexpected response of Send_if_cond command"); }

	/*
	 * At this point we could detect the unwanted case of an SDIO card
	 * beeing inserted by issuing 4 Io_send_op_cond commands at an
	 * interval of 10 ms (they should time out on SD).
	 */

	if (!issue_command(Sd_send_op_cond(0, false))) {
		_detect_err("Sd_send_op_cond command failed"); }

	_delayer.usleep(1000);
	if (!issue_command(Go_idle_state())) {
		_detect_err("Go_idle_state command failed"); }

	_delayer.usleep(2000);
	if (!issue_command(Send_if_cond())) {
		_detect_err("Send_if_cond failed"); }

	if (Mmio::read<Cmdrsp0>() != 0x1aa) {
		_detect_err("Unexpected response of Send_if_cond command"); }

	/*
	 * Power on card
	 *
	 * We need to issue the same Sd_send_op_cond command multiple
	 * times. The first time, we receive the status information. On
	 * subsequent attempts, the response tells us that the card is
	 * busy. Usually, the command is issued twice. We give up if the
	 * card is not reaching busy state after one second.
	 */
	int i = 1000;
	for (; i > 0; --i) {
		if (!issue_command(Sd_send_op_cond(0x200000, true))) {
			_detect_err("Sd_send_op_cond command failed"); }

		if (Ocr::Busy::get(Mmio::read<Cmdrsp0>())) { break; }
		_delayer.usleep(1000);
	}
	if (!i) { _detect_err("Could not power-on SD card"); }

	/* get basic information about the card */
	Card_info card_info = _detect();

	/*
	 * Configure working clock of host
	 *
	 * Host and card may be driven with a higher clock rate but
	 * checks (maybe read SSR/SCR, read switch, try frequencies) are
	 * necessary for that.
	 */
	_clock(CLOCK_OPERATIONAL);

	/*
	 * Configure card and host to use 4 data signals
	 *
	 * Host and card may be driven with a higher bus width but
	 * further checks (read SCR) are necessary for that.
	 */
	if (!issue_command(Set_bus_width(Set_bus_width::Arg::Bus_width::FOUR_BITS),
	                   card_info.rca()))
	{
		_detect_err("Set_bus_width(FOUR_BITS) command failed");
	}
	_bus_width(BUS_WIDTH_4);
	_delayer.usleep(10000);

	/* configure card to use given block size */
	if (!issue_command(Set_blocklen(_block_size()))) {
		_detect_err("Set_blocklen command failed"); }

	/* configure host buffer */
	Wml::access_t wml = Mmio::read<Wml>();
	_watermark_level(wml);
	Mmio::write<Wml>(wml);

	/* configure ADMA */
	Mmio::write<Proctl::Dmas>(Proctl::Dmas::ADMA2);

	/* configure interrupts for operational mode */
	_disable_irqs();
	Mmio::write<Irqstat>(~0);
	_enable_irqs();
	return card_info;
}


void Driver::_detect_err(char const * const err)
{
	error(err);
	throw Detection_failed();
}


int Driver::_reset()
{
	/* start reset */
	Mmio::write<Sysctl::Rsta>(1);
	_reset_amendments();

	/* wait for reset completion */
	try { wait_for(_delayer, Sysctl::Rsta::Equal(0)); }
	catch (Polling_timeout) {
		error("Reset timed out");
		return -1;
	}
	return 0;
}


void Driver::_disable_irqs()
{
	Mmio::write<Irqstaten>(0);
	Mmio::write<Irqsigen>(0);
}


void Driver::_enable_irqs()
{
	Irq::access_t irq = 0;
	Irq::Cc::set(irq, 1);
	Irq::Tc::set(irq, 1);
	Irq::Dint::set(irq, 1);
	Irq::Ctoe::set(irq, 1);
	Irq::Cce::set(irq, 1);
	Irq::Cebe::set(irq, 1);
	Irq::Cie::set(irq, 1);
	Irq::Dtoe::set(irq, 1);
	Irq::Dce::set(irq, 1);
	Irq::Debe::set(irq, 1);
	Irq::Ac12e::set(irq, 1);
	Irq::Dmae::set(irq, 1);
	Mmio::write<Irqstaten>(irq);
	Mmio::write<Irqsigen>(irq);
}


void Driver::_bus_width(Bus_width bus_width)
{
	switch (bus_width) {
	case BUS_WIDTH_1: Mmio::write<Proctl::Dtw>(Proctl::Dtw::_1BIT); break;
	case BUS_WIDTH_4: Mmio::write<Proctl::Dtw>(Proctl::Dtw::_4BIT); break;
	}
}


void Driver::_disable_clock()
{
	_disable_clock_preparation();
	Sysctl::access_t sysctl = Mmio::read<Sysctl>();
	Sysctl::Ipgen::set(sysctl, 0);
	Sysctl::Hcken::set(sysctl, 0);
	Sysctl::Peren::set(sysctl, 0);
	Sysctl::Dvs::set(sysctl, Sysctl::Dvs::DIV1);
	Sysctl::Sdclkfs::set(sysctl, Sysctl::Sdclkfs::DIV1);
	Mmio::write<Sysctl>(sysctl);
}


void Driver::_enable_clock(Clock_divider divider)
{
	Sysctl::access_t sysctl = Mmio::read<Sysctl>();
	Sysctl::Ipgen::set(sysctl, 1);
	Sysctl::Hcken::set(sysctl, 1);
	Sysctl::Peren::set(sysctl, 1);
	switch (divider) {
	case CLOCK_DIV_4:
		Sysctl::Dvs::set(sysctl, Sysctl::Dvs::DIV4);
		Sysctl::Sdclkfs::set(sysctl, Sysctl::Sdclkfs::DIV1);
		break;
	case CLOCK_DIV_8:
		Sysctl::Dvs::set(sysctl, Sysctl::Dvs::DIV4);
		Sysctl::Sdclkfs::set(sysctl, Sysctl::Sdclkfs::DIV2);
		break;
	case CLOCK_DIV_512:
		Sysctl::Dvs::set(sysctl, Sysctl::Dvs::DIV16);
		Sysctl::Sdclkfs::set(sysctl, Sysctl::Sdclkfs::DIV32);
		break;
	}
	Mmio::write<Sysctl>(sysctl);
	_enable_clock_finish();
	_delayer.usleep(1000);
}


void Driver::_clock(Clock clock)
{
	wait_for(_delayer, Prsstat::Sdstb::Equal(1));
	_disable_clock();
	_clock_finish(clock);
}


Driver::Driver(Env & env, Platform::Connection & platform)
:
	Driver_base(env.ram()),
	Platform::Device(platform),
	Platform::Device::Mmio<SIZE>(*static_cast<Platform::Device *>(this)),
	_env(env),
	_platform(platform)
{
	log("SD card detected");
	log("capacity: ", card_info().capacity_mb(), " MiB");
}


Driver::~Driver() { }
