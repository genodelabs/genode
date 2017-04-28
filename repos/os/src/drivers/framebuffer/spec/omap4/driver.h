/*
 * \brief  Frame-buffer driver for the OMAP4430 display-subsystem (HDMI)
 * \author Norman Feske
 * \author Martin Stein
 * \date   2012-06-01
 */

/*
 * Copyright (C) 2012-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <drivers/defs/panda.h>
#include <base/attached_io_mem_dataspace.h>
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

		enum Format { FORMAT_RGB565 };
		enum Output { OUTPUT_LCD, OUTPUT_HDMI };

	private:

		Genode::Env &_env;

		bool _init_lcd(addr_t phys_base);

		bool _init_hdmi(addr_t phys_base);

		struct Timer_delayer : Timer::Connection, Mmio::Delayer
		{
			Timer_delayer(Genode::Env &env) : Timer::Connection(env) { }

			/**
			 * Implementation of 'Delayer' interface
			 */
			void usleep(unsigned us)
			{
				Timer::Connection::usleep(us);
			}
		} _delayer { _env };

		/* display sub system registers */
		Attached_io_mem_dataspace _dss_mmio;
		Dss                       _dss;

		/* display controller registers */
		Attached_io_mem_dataspace _dispc_mmio;
		Dispc                     _dispc;

		/* HDMI controller registers */
		Attached_io_mem_dataspace _hdmi_mmio;
		Hdmi                      _hdmi;

		size_t _fb_width;
		size_t _fb_height;
		Format _fb_format;

	public:

		Driver(Genode::Env &env);

		static size_t bytes_per_pixel(Format format)
		{
			switch (format) {
			case FORMAT_RGB565: return 2;
			}
			return 0;
		}

		size_t buffer_size(size_t width, size_t height, Format format)
		{
			return bytes_per_pixel(format)*width*height;
		}

		bool init(size_t width, size_t height, Format format,
			Output output, addr_t phys_base);
};


Framebuffer::Driver::Driver(Genode::Env &env)
:
	_env(env),
	_dss_mmio(_env, Panda::DSS_MMIO_BASE, Panda::DSS_MMIO_SIZE),
	_dss((addr_t)_dss_mmio.local_addr<void>()),

	_dispc_mmio(_env, Panda::DISPC_MMIO_BASE, Panda::DISPC_MMIO_SIZE),
	_dispc((addr_t)_dispc_mmio.local_addr<void>()),

	_hdmi_mmio(_env, Panda::HDMI_MMIO_BASE, Panda::HDMI_MMIO_SIZE),
	_hdmi((addr_t)_hdmi_mmio.local_addr<void>()),

	_fb_width(0),
	_fb_height(0),
	_fb_format(FORMAT_RGB565)
{ }


bool Framebuffer::Driver::_init_lcd(Framebuffer::addr_t phys_base)
{
	/* disable LCD to allow editing configuration */
	_dispc.write<Dispc::Control1::Lcd_enable>(0);

	/* set load mode */
	_dispc.write<Dispc::Config1::Load_mode>(Dispc::Config1::Load_mode::DATA_EVERY_FRAME);

	_dispc.write<Dispc::Size_lcd::Width>(_fb_width - 1);
	_dispc.write<Dispc::Size_lcd::Height>(_fb_height - 1);

	Dispc::Gfx_attributes::access_t pixel_format = 0;
	switch (_fb_format) {
	case FORMAT_RGB565: pixel_format = Dispc::Gfx_attributes::Format::RGB16; break;
	}
	_dispc.write<Dispc::Gfx_attributes::Format>(pixel_format);

	_dispc.write<Dispc::Gfx_ba0>(phys_base);
	_dispc.write<Dispc::Gfx_ba1>(phys_base);

	_dispc.write<Dispc::Gfx_size::Sizex>(_fb_width - 1);
	_dispc.write<Dispc::Gfx_size::Sizey>(_fb_height - 1);

	_dispc.write<Dispc::Global_buffer>(0x6d2240);
	_dispc.write<Dispc::Gfx_attributes::Enable>(1);

	_dispc.write<Dispc::Control1::Lcd_enable>(1);
	_dispc.write<Dispc::Control1::Go_lcd>(1);

	return true;
}

bool Framebuffer::Driver::_init_hdmi(Framebuffer::addr_t phys_base)
{
	/* enable display core clock and set divider to 1 */
	_dispc.write<Dispc::Divisor::Lcd>(1);
	_dispc.write<Dispc::Divisor::Enable>(1);

	/* set load mode */
	_dispc.write<Dispc::Config1::Load_mode>(Dispc::Config1::Load_mode::DATA_EVERY_FRAME);

	_hdmi.write<Hdmi::Video_cfg::Start>(0);

	if (!_hdmi.issue_pwr_pll_command(Hdmi::Pwr_ctrl::ALL_OFF, _delayer)) {
		error("powering off HDMI timed out");
		return false;
	}

	if (!_hdmi.issue_pwr_pll_command(Hdmi::Pwr_ctrl::BOTH_ON_ALL_CLKS, _delayer)) {
		error("powering on HDMI timed out");
		return false;
	}

	if (!_hdmi.reset_pll(_delayer)) {
		error("resetting HDMI PLL timed out");
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
		error("HDMI PLL GO timed out");
		return false;
	}

	if (!_hdmi.issue_pwr_phy_command(Hdmi::Pwr_ctrl::LDOON, _delayer)) {
		error("HDMI Phy power on timed out");
		return false;
	}

	_hdmi.write<Hdmi::Txphy_tx_ctrl::Freqout>(1);
	_hdmi.write<Hdmi::Txphy_digital_ctrl>(0xf0000000);

	if (!_hdmi.issue_pwr_phy_command(Hdmi::Pwr_ctrl::TXON, _delayer)) {
		error("HDMI Txphy power on timed out");
		return false;
	}

	_hdmi.write<Hdmi::Video_timing_h::Bp>(160);
	_hdmi.write<Hdmi::Video_timing_h::Fp>(24);
	_hdmi.write<Hdmi::Video_timing_h::Sw>(136);

	_hdmi.write<Hdmi::Video_timing_v::Bp>(29);
	_hdmi.write<Hdmi::Video_timing_v::Fp>(3);
	_hdmi.write<Hdmi::Video_timing_v::Sw>(6);

	_hdmi.write<Hdmi::Video_cfg::Packing_mode>(Hdmi::Video_cfg::Packing_mode::PACK_24B);

	_hdmi.write<Hdmi::Video_size::X>(_fb_width);
	_hdmi.write<Hdmi::Video_size::Y>(_fb_height);

	_hdmi.write<Hdmi::Video_cfg::Vsp>(0);
	_hdmi.write<Hdmi::Video_cfg::Hsp>(0);
	_hdmi.write<Hdmi::Video_cfg::Interlacing>(0);
	_hdmi.write<Hdmi::Video_cfg::Tm>(1);

	_dss.write<Dss::Ctrl::Venc_hdmi_switch>(Dss::Ctrl::Venc_hdmi_switch::HDMI);

	_dispc.write<Dispc::Size_tv::Width>(_fb_width - 1);
	_dispc.write<Dispc::Size_tv::Height>(_fb_height - 1);

	_hdmi.write<Hdmi::Video_cfg::Start>(1);

	Dispc::Gfx_attributes::access_t pixel_format = 0;
	switch (_fb_format) {
	case FORMAT_RGB565: pixel_format = Dispc::Gfx_attributes::Format::RGB16; break;
	}
	_dispc.write<Dispc::Gfx_attributes::Format>(pixel_format);

	_dispc.write<Dispc::Gfx_ba0>(phys_base);
	_dispc.write<Dispc::Gfx_ba1>(phys_base);

	_dispc.write<Dispc::Gfx_size::Sizex>(_fb_width - 1);
	_dispc.write<Dispc::Gfx_size::Sizey>(_fb_height - 1);

	_dispc.write<Dispc::Global_buffer>(0x6d2240);
	_dispc.write<Dispc::Gfx_attributes::Enable>(1);

	_dispc.write<Dispc::Gfx_attributes::Channelout>(Dispc::Gfx_attributes::Channelout::TV);
	_dispc.write<Dispc::Gfx_attributes::Channelout2>(Dispc::Gfx_attributes::Channelout2::PRIMARY_LCD);

	_dispc.write<Dispc::Control1::Tv_enable>(1);
	_dispc.write<Dispc::Control1::Go_tv>(1);

	if (!_dispc.wait_for<Dispc::Control1::Go_tv>(Dispc::Control1::Go_tv::HW_UPDATE_DONE, _delayer)) {
		error("Go_tv timed out");
		return false;
	}

	return true;
}


bool Framebuffer::Driver::init(size_t width, size_t height,
                               Framebuffer::Driver::Format format,
                               Output output,
                               Framebuffer::addr_t phys_base)
{
	_fb_width = width;
	_fb_height = height;
	_fb_format = format;

	bool ret = false;
	switch (output) {
		case OUTPUT_LCD:
			ret = _init_lcd(phys_base);
			break;
		case OUTPUT_HDMI:
			ret = _init_hdmi(phys_base);
			break;
		default:
			error("unknown output ", (int)output, " specified");
	}
	return ret;
}
