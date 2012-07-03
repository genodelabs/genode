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
#include <os/attached_io_mem_dataspace.h>
#include <timer_session/connection.h>
#include <util/mmio.h>

/* local includes */
#include <dss.h>
#include <dispc.h>
#include <hdmi.h>


namespace Framebuffer {
	using namespace Genode;
	class Driver;
}


class Framebuffer::Driver
{
	public:

		enum Mode { MODE_1024_768 };
		enum Format { FORMAT_RGB565 };

	private:

		struct Timer_delayer : Timer::Connection, Mmio::Delayer
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
		} _delayer;

		/* memory map */
		enum {
			DSS_MMIO_BASE   = 0x58000000,
			DSS_MMIO_SIZE   = 0x00001000,

			DISPC_MMIO_BASE = 0x58001000,
			DISPC_MMIO_SIZE = 0x1000,

			HDMI_MMIO_BASE  = 0x58006000,
			HDMI_MMIO_SIZE  = 0x1000,
		};

		/* display sub system registers */
		Attached_io_mem_dataspace _dss_mmio;
		Dss                       _dss;

		/* display controller registers */
		Attached_io_mem_dataspace _dispc_mmio;
		Dispc                     _dispc;

		/* HDMI controller registers */
		Attached_io_mem_dataspace _hdmi_mmio;
		Hdmi                      _hdmi;

	public:

		Driver();

		static size_t bytes_per_pixel(Format format)
		{
			switch (format) {
			case FORMAT_RGB565: return 2;
			}
			return 0;
		}

		static size_t width(Mode mode)
		{
			switch (mode) {
			case MODE_1024_768: return 1024;
			}
			return 0;
		}

		static size_t height(Mode mode)
		{
			switch (mode) {
			case MODE_1024_768: return 768;
			}
			return 0;
		}

		static size_t buffer_size(Mode mode, Format format)
		{
			return bytes_per_pixel(format)*width(mode)*height(mode);
		}

		bool init(Mode mode, Format format, addr_t phys_base);
};


Framebuffer::Driver::Driver()
:
	_dss_mmio(DSS_MMIO_BASE, DSS_MMIO_SIZE),
	_dss((addr_t)_dss_mmio.local_addr<void>()),

	_dispc_mmio(DISPC_MMIO_BASE, DISPC_MMIO_SIZE),
	_dispc((addr_t)_dispc_mmio.local_addr<void>()),

	_hdmi_mmio(HDMI_MMIO_BASE, HDMI_MMIO_SIZE),
	_hdmi((addr_t)_hdmi_mmio.local_addr<void>())
{ }


bool Framebuffer::Driver::init(Framebuffer::Driver::Mode   mode,
                               Framebuffer::Driver::Format format,
                               Framebuffer::addr_t         phys_base)
{
	/* enable display core clock and set divider to 1 */
	_dispc.write<Dispc::Divisor::Lcd>(1);
	_dispc.write<Dispc::Divisor::Enable>(1);

	/* set load mode */
	_dispc.write<Dispc::Config1::Load_mode>(Dispc::Config1::Load_mode::DATA_EVERY_FRAME);

	_hdmi.write<Hdmi::Video_cfg::Start>(0);

	if (!_hdmi.issue_pwr_pll_command(Hdmi::Pwr_ctrl::ALL_OFF, _delayer)) {
		PERR("Powering off HDMI timed out\n");
		return false;
	}

	if (!_hdmi.issue_pwr_pll_command(Hdmi::Pwr_ctrl::BOTH_ON_ALL_CLKS, _delayer)) {
		PERR("Powering on HDMI timed out\n");
		return false;
	}

	if (!_hdmi.reset_pll(_delayer)) {
		PERR("Resetting HDMI PLL timed out\n");
		return false;
	}

	_hdmi.write<Hdmi::Pll_control::Mode>(Hdmi::Pll_control::Mode::MANUAL);

	_hdmi.write<Hdmi::Cfg1::Regm>(270);
	_hdmi.write<Hdmi::Cfg1::Regn>(15);

	_hdmi.write<Hdmi::Cfg2::Highfreq_div_by_2>(0);
	_hdmi.write<Hdmi::Cfg2::Refen>(1);
	_hdmi.write<Hdmi::Cfg2::Clkinen>(0);
	_hdmi.write<Hdmi::Cfg2::Refsel>(3);
	_hdmi.write<Hdmi::Cfg2::Freq_divider>(2);

	_hdmi.write<Hdmi::Cfg4::Regm2>(1);
	_hdmi.write<Hdmi::Cfg4::Regmf>(0x35555);

	if (!_hdmi.pll_go(_delayer)) {
		PERR("HDMI PLL GO timed out");
		return false;
	}

	if (!_hdmi.issue_pwr_phy_command(Hdmi::Pwr_ctrl::LDOON, _delayer)) {
		PERR("HDMI Phy power on timed out");
		return false;
	}

	_hdmi.write<Hdmi::Txphy_tx_ctrl::Freqout>(1);
	_hdmi.write<Hdmi::Txphy_digital_ctrl>(0xf0000000);

	if (!_hdmi.issue_pwr_phy_command(Hdmi::Pwr_ctrl::TXON, _delayer)) {
		PERR("HDMI Txphy power on timed out");
		return false;
	}

	_hdmi.write<Hdmi::Video_timing_h::Bp>(160);
	_hdmi.write<Hdmi::Video_timing_h::Fp>(24);
	_hdmi.write<Hdmi::Video_timing_h::Sw>(136);

	_hdmi.write<Hdmi::Video_timing_v::Bp>(29);
	_hdmi.write<Hdmi::Video_timing_v::Fp>(3);
	_hdmi.write<Hdmi::Video_timing_v::Sw>(6);

	_hdmi.write<Hdmi::Video_cfg::Packing_mode>(Hdmi::Video_cfg::Packing_mode::PACK_24B);

	_hdmi.write<Hdmi::Video_size::X>(width(mode));
	_hdmi.write<Hdmi::Video_size::Y>(height(mode));

	_hdmi.write<Hdmi::Video_cfg::Vsp>(0);
	_hdmi.write<Hdmi::Video_cfg::Hsp>(0);
	_hdmi.write<Hdmi::Video_cfg::Interlacing>(0);
	_hdmi.write<Hdmi::Video_cfg::Tm>(1);

	_dss.write<Dss::Ctrl::Venc_hdmi_switch>(Dss::Ctrl::Venc_hdmi_switch::HDMI);

	_dispc.write<Dispc::Size_tv::Width>(width(mode) - 1);
	_dispc.write<Dispc::Size_tv::Height>(height(mode) - 1);

	_hdmi.write<Hdmi::Video_cfg::Start>(1);

	Dispc::Gfx_attributes::access_t pixel_format = 0;
	switch (format) {
	case FORMAT_RGB565: pixel_format = Dispc::Gfx_attributes::Format::RGB16; break;
	}
	_dispc.write<Dispc::Gfx_attributes::Format>(pixel_format);

	_dispc.write<Dispc::Gfx_ba1>(phys_base);

	_dispc.write<Dispc::Gfx_size::Sizex>(width(mode) - 1);
	_dispc.write<Dispc::Gfx_size::Sizey>(height(mode) - 1);

	_dispc.write<Dispc::Global_buffer>(0x6d2240);
	_dispc.write<Dispc::Gfx_attributes::Enable>(1);

	_dispc.write<Dispc::Gfx_attributes::Channelout>(Dispc::Gfx_attributes::Channelout::TV);
	_dispc.write<Dispc::Gfx_attributes::Channelout2>(Dispc::Gfx_attributes::Channelout2::PRIMARY_LCD);

	_dispc.write<Dispc::Control1::Tv_enable>(1);

	_dispc.write<Dispc::Control1::Go_tv>(1);

	if (!_dispc.wait_for<Dispc::Control1::Go_tv>(Dispc::Control1::Go_tv::HW_UPDATE_DONE, _delayer)) {
		PERR("Go_tv timed out");
		return false;
	}

	return true;
}
