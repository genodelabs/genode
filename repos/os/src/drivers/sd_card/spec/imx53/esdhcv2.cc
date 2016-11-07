/*
 * \brief  Freescale Enhanced Secured Digital Host Controller Version 2
 * \author Martin Stein
 * \date   2015-02-05
 */

/*
 * Copyright (C) 2015 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* local includes */
#include <esdhcv2.h>

using namespace Sd_card;
using namespace Genode;


int Esdhcv2_controller::_wait_for_card_ready_mbw()
{
	/*
	 * Poll card status
	 *
	 * The maximum number of attempts and the delay between two attempts are
	 * freely chosen.
	 */
	unsigned           attempts          = 5;
	unsigned constexpr attempts_delay_us = 100000;
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
		write<Cmdarg>(cmdarg);
		write<Xfertyp>(xfertyp);

		/* wait for command completion */
		if (_wait_for_cmd_complete()) { return -1; }

		/* check for errors */
		R1_response_0::access_t const resp = read<Cmdrsp0>();
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


int Esdhcv2_controller::_stop_transmission_mbw()
{
	/* write argument register */
	write<Cmdarg>(0);

	/* write command register */
	Xfertyp::access_t xfertyp = 0;
	Xfertyp::Cmdinx::set(xfertyp, Stop_transmission::INDEX);
	Xfertyp::Cmdtyp::set(xfertyp, Xfertyp::Cmdtyp::ABORT_CMD12);
	Xfertyp::Cccen::set(xfertyp, 1);
	Xfertyp::Cicen::set(xfertyp, 1);
	Xfertyp::Rsptyp::set(xfertyp, Xfertyp::Rsptyp::_48BIT_BUSY);
	Xfertyp::Msbsel::set(xfertyp, 1);
	Xfertyp::Bcen::set(xfertyp, 1);
	Xfertyp::Dmaen::set(xfertyp, 1);
	write<Xfertyp>(xfertyp);

	/* wait for command completion */
	if (_wait_for_cmd_complete()) { return -1; }
	return 0;
}


int Esdhcv2_controller::_wait_for_cmd_complete_mb(bool const r)
{
	/*
	 * The ESDHC signals on multi-block transfers seem to be broken.
	 * Synchronizing to "Transfer Complete" before returning from transfers
	 * and to "Command Inhibit" before sending further commands - as it is
	 * done with other controllers - isn't sufficient. Instead, both "Transfer
	 * Complete" and "Command Complete" must be gathered.
	 */
	Irqstat::access_t constexpr irq_goal =
		Irq::Cc::reg_mask() | Irq::Tc::reg_mask();

	/* wait for a first signal */
	_wait_for_irq();
	Irqstat::access_t const irq = read<Irqstat>();

	/*
	 * Poll for missing signal because interrupts are edge-triggered
	 * and could thus got lost in the meantime.
	 */
	if (irq != irq_goal) {
		if (!wait_for<Irqstat>(irq_goal, _delayer)) {
			error("Completion host signal timed out");
			return -1;
		}
	}
	/* acknowledge completion signals */
	write<Irqstat>(irq_goal);
	if (!r) {

		/*
		 * The "Auto Command 12" feature of the ESDHC seems to be
		 * broken for multi-block writes as it causes command-
		 * timeout errors sometimes. Thus, we stop such transfers
		 * manually.
		 */
		if (_stop_transmission_mbw())  { return -1; }

		/*
		 * The manual termination of multi-block writes seems to leave
		 * the card in a busy state sometimes. This causes
		 * errors on subsequent commands. Thus, we have to synchronize
		 * manually with the card-internal state.
		 */
		if (_wait_for_card_ready_mbw()) { return -1; }
	}
	return 0;
}


int Esdhcv2_controller::_wait_for_cmd_complete()
{
	/* wait for "Command Completion" signal and acknowledge it */
	_wait_for_irq();
	if (read<Irqstat>() != Irqstat::Cc::reg_mask()) {
		error("received unexpected host signal");
		return -1;
	}
	write<Irqstat>(Irqstat::Cc::reg_mask());
	return 0;
}


bool Esdhcv2_controller::_issue_command(Command_base const & command)
{
	/* detect if command is a multi-block transfer and whether it reads */
	bool const r = command.transfer == TRANSFER_READ;
	bool const mb =
		command.index == Read_multiple_block::INDEX ||
		command.index == Write_multiple_block::INDEX;

	/* assemble command register value */
	Xfertyp::access_t cmd = 0;
	Xfertyp::Cmdinx::set(cmd, command.index);
	if (command.transfer != TRANSFER_NONE) {
		Xfertyp::Dpsel::set(cmd);
		Xfertyp::Bcen::set(cmd);
		Xfertyp::Msbsel::set(cmd);
		if (mb) {
			/*
			 * The "Auto Command 12" feature of the ESDHC seems to be
			 * broken for multi-block writes as it causes command-
			 * timeout errors sometimes.
			 */
			if (r) { Xfertyp::Ac12en::set(cmd); }
			if (_use_dma) { Xfertyp::Dmaen::set(cmd); }
		}
		Xfertyp::Dtdsel::set(cmd,
			r ? Xfertyp::Dtdsel::READ : Xfertyp::Dtdsel::WRITE);
	}
	typedef Xfertyp::Rsptyp Rsptyp;
	Xfertyp::access_t rt = 0;
	switch (command.rsp_type) {
	case RESPONSE_NONE:             rt = Rsptyp::_0BIT;       break;
	case RESPONSE_136_BIT:          rt = Rsptyp::_136BIT;     break;
	case RESPONSE_48_BIT:           rt = Rsptyp::_48BIT;      break;
	case RESPONSE_48_BIT_WITH_BUSY: rt = Rsptyp::_48BIT_BUSY; break;
	}
	Xfertyp::Rsptyp::set(cmd, rt);

	/* send command as soon as the host allows it */
	if (_wait_for_cmd_allowed()) { return false; }
	write<Cmdarg>(command.arg);
	write<Xfertyp>(cmd);

	/* wait for completion */
	return mb ? !_wait_for_cmd_complete_mb(r) : !_wait_for_cmd_complete();
}


Cid Esdhcv2_controller::_read_cid()
{
	Cid cid;
	cid.raw_0 = read<Rsp136_0>();
	cid.raw_1 = read<Rsp136_1>();
	cid.raw_2 = read<Rsp136_2>();
	cid.raw_3 = read<Rsp136_3>();
	return cid;
}


Csd Esdhcv2_controller::_read_csd()
{
	Csd csd;
	csd.csd0 = read<Rsp136_0>();
	csd.csd1 = read<Rsp136_1>();
	csd.csd2 = read<Rsp136_2>();
	csd.csd3 = read<Rsp136_3>();
	return csd;
}


unsigned Esdhcv2_controller::_read_rca()
{
	Cmdrsp0::access_t const rsp0 = read<Cmdrsp0>();
	return Send_relative_addr::Response::Rca::get(rsp0);
}


bool Esdhcv2_controller::read_blocks(size_t, size_t, char *)
{
	error("block transfer without DMA not supported by now");
	return false;
}


bool Esdhcv2_controller::write_blocks(size_t, size_t, char const *)
{
	error("block transfer without DMA not supported by now");
	return false;
}


bool Esdhcv2_controller::read_blocks_dma(size_t blk_nr, size_t blk_cnt,
                                         addr_t buf_phys)
{
	if (_prepare_dma_mb(blk_cnt, buf_phys)) { return false; }
	return issue_command(Read_multiple_block(blk_nr));
}


bool Esdhcv2_controller::write_blocks_dma(size_t blk_nr, size_t blk_cnt,
                                          addr_t buf_phys)
{
	if (_prepare_dma_mb(blk_cnt, buf_phys)) { return false; }
	return issue_command(Write_multiple_block(blk_nr));
}


Esdhcv2_controller::Esdhcv2_controller(addr_t const base, unsigned const irq,
                                       Delayer & delayer, bool const use_dma)
:
	Esdhcv2(base), _irq(irq), _delayer(delayer), _card_info(_init()),
	_use_dma(use_dma)
{ }


int Esdhcv2_controller::_prepare_dma_mb(size_t blk_cnt, addr_t buf_phys)
{
	/* write ADMA2 table to DMA */
	size_t const req_size = blk_cnt * BLOCK_SIZE;
	if (_adma2_table.setup_request(req_size, buf_phys)) { return -1; }

	/* configure DMA at host */
	write<Adsaddr>(_adma2_table.base_phys());
	write<Blkattr::Blksize>(BLOCK_SIZE);
	write<Blkattr::Blkcnt>(blk_cnt);
	return 0;
}


int Esdhcv2_controller::_wait_for_cmd_allowed()
{
	/*
	 * At least after multi-block writes with the fix for the broken "Auto
	 * Command 12", waiting only for "Command Inhibit" isn't sufficient as
	 * "Data Line Active" and "Data Inhibit" may also be active.
	 */
	if (!wait_for<Prsstat_lhw>(Prsstat_lhw::cmd_allowed(), _delayer)) {
		error("wait till issuing a new command is allowed timed out");
		return -1;
	}
	return 0;
}


void Esdhcv2_controller::_wait_for_irq()
{
	/* acknowledge IRQ first, to activate IRQ propagation initially */
	_irq.ack_irq();
	_irq_rec.wait_for_signal();
}


Card_info Esdhcv2_controller::_init()
{
	/* install IRQ signal */
	_irq.sigh(_irq_rec.manage(&_irq_ctx));

	/* configure host for initialization stage */
	if (_reset(_delayer)) { _detect_err("Host reset failed"); }
	_disable_irqs();

	/* check host version */
	Hostver::access_t const hostver = read<Hostver>();
	if (Hostver::Vvn::get(hostver) != 18) {
		_detect_err("Unexpected Vendor Version Number"); }
	if (Hostver::Svn::get(hostver) != 1) {
		_detect_err("Unexpected Specification Version Number"); }

	/*
	 * We should check host capabilities at this point if we want to
	 * support other versions of the ESDHC. For the i.MX53 ESDHCv2 we
	 * know that the capabilities fit our requirements.
	 */

	/* configure IRQs, bus width, and clock for initialization */
	_enable_irqs();
	_bus_width(BUS_WIDTH_1);
	_delayer.usleep(10000);
	_clock(CLOCK_DIV_512, _delayer);

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

	if (read<Cmdrsp0>() != 0x1aa) {
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

	if (read<Cmdrsp0>() != 0x1aa) {
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

		if (Ocr::Busy::get(read<Cmdrsp0>())) { break; }
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
	_clock(CLOCK_DIV_8, _delayer);

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
	if (!issue_command(Set_blocklen(BLOCK_SIZE))) {
		_detect_err("Set_blocklen command failed"); }

	/* configure host buffer */
	Wml::access_t wml = read<Wml>();
	Wml::Rd_wml::set(wml, WATERMARK_WORDS);
	Wml::Rd_brst_len::set(wml, BURST_WORDS);
	Wml::Wr_wml::set(wml, WATERMARK_WORDS);
	Wml::Wr_brst_len::set(wml, BURST_WORDS);
	write<Wml>(wml);

	/* configure ADMA */
	write<Proctl::Dmas>(Proctl::Dmas::ADMA2);

	/* configure interrupts for operational mode */
	_disable_irqs();
	write<Irqstat>(~0);
	_enable_irqs();
	return card_info;
}


void Esdhcv2_controller::_detect_err(char const * const err)
{
	error(err);
	throw Detection_failed();
}


int Esdhcv2_controller::_reset(Delayer & delayer)
{
	/* start reset */
	write<Sysctl::Rsta>(1);

	/*
	 * The SDHC specification says that a software reset shouldn't
	 * have an effect on the the card detection circuit. The ESDHC
	 * clears Sysctl::Ipgen, Sysctl::Hcken, and Sysctl::Peren
	 * nonetheless which disables clocks that card detection relies
	 * on.
	 */
	Sysctl::access_t sysctl = read<Sysctl>();
	Sysctl::Ipgen::set(sysctl, 1);
	Sysctl::Hcken::set(sysctl, 1);
	Sysctl::Peren::set(sysctl, 1);
	write<Sysctl>(sysctl);

	/* wait for reset completion */
	if (!wait_for<Sysctl::Rsta>(0, delayer)) {
		error("Reset timed out");
		return -1;
	}
	return 0;
}


void Esdhcv2_controller::_disable_irqs()
{
	write<Irqstaten>(0);
	write<Irqsigen>(0);
}


void Esdhcv2_controller::_enable_irqs()
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
	write<Irqstaten>(irq);
	write<Irqsigen>(irq);
}


void Esdhcv2_controller::_bus_width(Bus_width bus_width)
{
	switch (bus_width) {
	case BUS_WIDTH_1: write<Proctl::Dtw>(Proctl::Dtw::_1BIT); break;
	case BUS_WIDTH_4: write<Proctl::Dtw>(Proctl::Dtw::_4BIT); break;
	}
}


void Esdhcv2_controller::_disable_clock()
{
	Sysctl::access_t sysctl = read<Sysctl>();
	Sysctl::Ipgen::set(sysctl, 0);
	Sysctl::Hcken::set(sysctl, 0);
	Sysctl::Peren::set(sysctl, 0);
	Sysctl::Dvs::set(sysctl, Sysctl::Dvs::DIV1);
	Sysctl::Sdclkfs::set(sysctl, Sysctl::Sdclkfs::DIV1);
	write<Sysctl>(sysctl);
}


void Esdhcv2_controller::_enable_clock(Clock_divider divider, Delayer &delayer)
{
	Sysctl::access_t sysctl = read<Sysctl>();
	Sysctl::Ipgen::set(sysctl, 1);
	Sysctl::Hcken::set(sysctl, 1);
	Sysctl::Peren::set(sysctl, 1);
	switch (divider) {
	case CLOCK_DIV_8:
		Sysctl::Dvs::set(sysctl, Sysctl::Dvs::DIV4);
		Sysctl::Sdclkfs::set(sysctl, Sysctl::Sdclkfs::DIV2);
		break;
	case CLOCK_DIV_512:
		Sysctl::Dvs::set(sysctl, Sysctl::Dvs::DIV16);
		Sysctl::Sdclkfs::set(sysctl, Sysctl::Sdclkfs::DIV32);
		break;
	}
	write<Sysctl>(sysctl);
	delayer.usleep(1000);
}


void Esdhcv2_controller::_clock(enum Clock_divider divider, Delayer &delayer)
{
	_disable_clock();
	write<Sysctl::Dtocv>(Sysctl::Dtocv::SDCLK_TIMES_2_POW_27);
	_enable_clock(divider, delayer);
}
