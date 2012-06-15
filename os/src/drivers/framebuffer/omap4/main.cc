/*
 * \brief  Frame-buffer driver for the OMAP4430 display-subsystem (HDMI)
 * \author Norman Feske
 * \author Martin Stein
 * \date   2012-06-01
 */

/*
 * Copyright (C) 2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <framebuffer_session/framebuffer_session.h>
#include <os/attached_io_mem_dataspace.h>
#include <timer_session/connection.h>
#include <cap_session/connection.h>
#include <dataspace/client.h>
#include <base/printf.h>
#include <base/sleep.h>
#include <root/component.h>

/* local includes */
#include <mmio.h>
#include <dss.h>
#include <dispc.h>
#include <hdmi.h>


int main(int, char **)
{
	using namespace Genode;
	printf("--- omap44xx framebuffer driver ---\n");

	struct Timer_delayer : Timer::Connection, Delayer
	{
		/**
		 * Implementation of 'Delayer' interface
		 */
		void usleep(unsigned us)
		{
			/* polling */
			if (us == 0)
				return;

			unsigned ms = us / 1000;
			if (ms == 0)
				ms = 1;

			Timer::Connection::msleep(ms);
		}
	};

	static Timer_delayer delayer;

	/* memory map */
	enum {
		DSS_MMIO_BASE  = 0x58000000,
		DSS_MMIO_SIZE  = 0x00001000,

		DISPC_MMIO_BASE = 0x58001000,
		DISPC_MMIO_SIZE = 0x1000,

		HDMI_MMIO_BASE = 0x58006000,
		HDMI_MMIO_SIZE = 0x1000,
	};

	/*
	 * Obtain MMIO resources
	 */

	/* display sub system registers */
	Attached_io_mem_dataspace dss_mmio(DSS_MMIO_BASE, DSS_MMIO_SIZE);
	Dss dss((addr_t)dss_mmio.local_addr<void>());

	/* display controller registers */
	Attached_io_mem_dataspace dispc_mmio(DISPC_MMIO_BASE, DISPC_MMIO_SIZE);
	Dispc dispc((addr_t)dispc_mmio.local_addr<void>());

	/* HDMI controller registers */
	Attached_io_mem_dataspace hdmi_mmio(HDMI_MMIO_BASE, HDMI_MMIO_SIZE);
	Hdmi hdmi((addr_t)hdmi_mmio.local_addr<void>());

	/* enable display core clock and set divider to 1 */
	dispc.write<Dispc::Divisor::Lcd>(1);
	dispc.write<Dispc::Divisor::Enable>(1);

	/* set load mode */
	dispc.write<Dispc::Config1::Load_mode>(Dispc::Config1::Load_mode::DATA_EVERY_FRAME);

	hdmi.write<Hdmi::Video_cfg::Start>(0);

	if (!hdmi.issue_pwr_pll_command(Hdmi::Pwr_ctrl::ALL_OFF, delayer)) {
		PERR("Powering off HDMI timed out\n");
		return 1;
	}

	if (!hdmi.issue_pwr_pll_command(Hdmi::Pwr_ctrl::BOTH_ON_ALL_CLKS, delayer)) {
		PERR("Powering on HDMI timed out\n");
		return 2;
	}

	if (!hdmi.reset_pll(delayer)) {
		PERR("Resetting HDMI PLL timed out\n");
		return 3;
	}

	hdmi.write<Hdmi::Pll_control::Mode>(Hdmi::Pll_control::Mode::MANUAL);

	hdmi.write<Hdmi::Cfg1::Regm>(270);
	hdmi.write<Hdmi::Cfg1::Regn>(15);

	hdmi.write<Hdmi::Cfg2::Highfreq_div_by_2>(0);
	hdmi.write<Hdmi::Cfg2::Refen>(1);
	hdmi.write<Hdmi::Cfg2::Clkinen>(0);
	hdmi.write<Hdmi::Cfg2::Refsel>(3);
	hdmi.write<Hdmi::Cfg2::Freq_divider>(2);

	hdmi.write<Hdmi::Cfg4::Regm2>(1);
	hdmi.write<Hdmi::Cfg4::Regmf>(0x35555);

	if (!hdmi.pll_go(delayer)) {
		PERR("HDMI PLL GO timed out");
		return 4;
	}

	if (!hdmi.issue_pwr_phy_command(Hdmi::Pwr_ctrl::LDOON, delayer)) {
		PERR("HDMI Phy power on timed out");
		return 5;
	}

	hdmi.write<Hdmi::Txphy_tx_ctrl::Freqout>(1);
	hdmi.write<Hdmi::Txphy_digital_ctrl>(0xf0000000);

	if (!hdmi.issue_pwr_phy_command(Hdmi::Pwr_ctrl::TXON, delayer)) {
		PERR("HDMI Txphy power on timed out");
		return 6;
	}

	hdmi.write<Hdmi::Video_timing_h::Bp>(160);
	hdmi.write<Hdmi::Video_timing_h::Fp>(24);
	hdmi.write<Hdmi::Video_timing_h::Sw>(136);

	hdmi.write<Hdmi::Video_timing_v::Bp>(29);
	hdmi.write<Hdmi::Video_timing_v::Fp>(3);
	hdmi.write<Hdmi::Video_timing_v::Sw>(6);

	hdmi.write<Hdmi::Video_cfg::Packing_mode>(Hdmi::Video_cfg::Packing_mode::PACK_24B);

	hdmi.write<Hdmi::Video_size::X>(1024);
	hdmi.write<Hdmi::Video_size::Y>(768);

	hdmi.write<Hdmi::Video_cfg::Vsp>(0);
	hdmi.write<Hdmi::Video_cfg::Hsp>(0);
	hdmi.write<Hdmi::Video_cfg::Interlacing>(0);
	hdmi.write<Hdmi::Video_cfg::Tm>(1);

	dss.write<Dss::Ctrl::Venc_hdmi_switch>(Dss::Ctrl::Venc_hdmi_switch::HDMI);

	dispc.write<Dispc::Size_tv::Width>(1024 - 1);
	dispc.write<Dispc::Size_tv::Height>(768 - 1);

	hdmi.write<Hdmi::Video_cfg::Start>(1);

	dispc.write<Dispc::Gfx_attributes::Format>(Dispc::Gfx_attributes::Format::RGB16);

	typedef Genode::uint16_t pixel_t;

	Genode::Dataspace_capability fb_ds =
		Genode::env()->ram_session()->alloc(1024*768*sizeof(pixel_t), false);

	Genode::addr_t fb_phys_base = Genode::Dataspace_client(fb_ds).phys_addr();

	dispc.write<Dispc::Gfx_ba1>(fb_phys_base);

	dispc.write<Dispc::Gfx_size::Sizex>(1024 - 1);
	dispc.write<Dispc::Gfx_size::Sizey>(768 - 1);

	dispc.write<Dispc::Global_buffer>(0x6d2240);
	dispc.write<Dispc::Gfx_attributes::Enable>(1);

	dispc.write<Dispc::Gfx_attributes::Channelout>(Dispc::Gfx_attributes::Channelout::TV);
	dispc.write<Dispc::Gfx_attributes::Channelout2>(Dispc::Gfx_attributes::Channelout2::PRIMARY_LCD);

	dispc.write<Dispc::Control1::Tv_enable>(1);

	dispc.write<Dispc::Control1::Go_tv>(1);

	if (!dispc.wait_for<Dispc::Control1::Go_tv>(Dispc::Control1::Go_tv::HW_UPDATE_DONE, delayer)) {
		PERR("Go_tv timed out");
		return 6;
	}

	pixel_t *fb_local_base = Genode::env()->rm_session()->attach(fb_ds);
	for (unsigned y = 0; y < 768; y++) {
		for (unsigned x = 0; x < 1024; x++) {
			fb_local_base[y*1024 + x] = (x & 0x1f);
			fb_local_base[y*1024 + x] |= ((y) & 0x1f) << 11;
			fb_local_base[y*1024 + x] |= ((x + y) & 0x3f) << 5;
		}
	}

	PINF("done");
	sleep_forever();
	return 0;
}

