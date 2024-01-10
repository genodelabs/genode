/*
 * \brief  PL11x frame-buffer driver
 * \author Norman Feske
 * \author Stefan Kalkowski
 * \date   2010-02-17
 */

/*
 * Copyright (C) 2010-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <base/attached_dataspace.h>
#include <base/component.h>
#include <base/log.h>
#include <platform_session/device.h>
#include <platform_session/dma_buffer.h>
#include <timer_session/connection.h>
#include <capture_session/connection.h>
#include <blit/painter.h>
#include <os/pixel_rgb888.h>

namespace Pl11x_driver
{
	enum {
		SCR_WIDTH    =  640,
		SCR_HEIGHT   =  480,
		LEFT_MARGIN  =   64,
		RIGHT_MARGIN =   32,
		UPPER_MARGIN =    9,
		LOWER_MARGIN =   11,
		HSYNC_LEN    =   64,
		VSYNC_LEN    =   25,

		BYTES_PER_PIXEL  = 4,
		FRAMEBUFFER_SIZE = SCR_WIDTH*SCR_HEIGHT*BYTES_PER_PIXEL,
	};

	using namespace Genode;
	class Main;
}


struct Pl11x_driver::Main
{
	using Area = Capture::Area;

	Env &_env;

	Area const _size { SCR_WIDTH, SCR_HEIGHT };


	/*
	 * Capture
	 */

	Capture::Connection _capture { _env };

	Capture::Connection::Screen _captured_screen { _capture, _env.rm(), _size };


	/*
	 * Timer
	 */

	Timer::Connection _timer { _env };

	Signal_handler<Main> _timer_handler { _env.ep(), *this, &Main::_handle_timer };

	void _handle_timer()
	{
		using Pixel = Capture::Pixel;

		Surface<Pixel> surface(_fb_dma.local_addr<Pixel>(), _size);

		_captured_screen.apply_to_surface(surface);
	}


	/*
	 * Driver
	 */

	using Type = Platform::Device::Type;

	Platform::Connection      _platform  { _env };
	Platform::Device          _pl11x_dev { _platform, Type { "arm,pl111" } };
	Platform::Device          _sp810_dev { _platform, Type { "arm,sp810" } };
	Platform::Device::Mmio<0> _lcd_io_mem { _pl11x_dev };
	Platform::Device::Mmio<0> _sys_mem    { _sp810_dev };
	Platform::Dma_buffer      _fb_dma     { _platform, FRAMEBUFFER_SIZE, UNCACHED };

	void _init_device();

	Main(Env &env) : _env(env)
	{
		_init_device();

		_timer.sigh(_timer_handler);
		_timer.trigger_periodic(20*1000);
	}
};


void Pl11x_driver::Main::_init_device()
{
	enum {
		/**
		 * Bit definitions of the lcd control register
		 */
		CTRL_ENABLED = 1 <<  0,
		CTRL_BPP_24  = 5 <<  1,
		CTRL_TFT     = 1 <<  5,
		CTRL_BGR     = 1 <<  8,
		CTRL_POWER   = 1 << 11,
		CTRL_VCOMP   = 1 << 12,

		/**
		 * Bit definitions for CLCDC timing.
		 */
		CLCDC_IVS     = 1 << 11,
		CLCDC_IHS     = 1 << 12,
		CLCDC_BCD     = 1 << 26,
	};

	enum Sp810_defs {
		SP810_REG_OSCCLCD = 0x1c,
		SP810_REG_LOCK    = 0x20,
	};

	enum Pl11x_defs {
		PL11X_REG_TIMING0 = 0,
		PL11X_REG_TIMING1 = 1,
		PL11X_REG_TIMING2 = 2,
		PL11X_REG_TIMING3 = 3,
		PL11X_REG_UPBASE  = 4,
		PL11X_REG_LPBASE  = 5,
		PL11X_REG_CTRL    = 6,
		PL11X_REG_IMSC    = 7,
	};

	addr_t const regs_base     = (addr_t)_lcd_io_mem.local_addr<void*>();
	addr_t const sys_regs_base = (addr_t)_sys_mem.local_addr<void*>();

	auto sys_reg_write = [&] (unsigned reg, long value) {
		*(volatile long *)(sys_regs_base + sizeof(long)*reg) = value; };

	auto reg_write = [&] (unsigned reg, long value) {
		*(volatile long *)(regs_base + sizeof(long)*reg) = value; };

	auto reg_read = [&] (unsigned reg) {
		return *(volatile long *)(regs_base + sizeof(long)*reg); };

	uint32_t const tim0 = (SCR_WIDTH/16 - 1) << 2
	                    | (HSYNC_LEN    - 1) << 8
	                    | (RIGHT_MARGIN - 1) << 16
	                    | (LEFT_MARGIN  - 1) << 24;
	uint32_t const tim1 = (SCR_HEIGHT - 1)
	                    | (VSYNC_LEN - 1)    << 10
	                    |  LOWER_MARGIN      << 16
	                    |  UPPER_MARGIN      << 24;
	uint32_t const tim2 = ((SCR_WIDTH - 1)   << 16)
	                    | CLCDC_IVS | CLCDC_IHS | CLCDC_BCD;
	uint32_t const tim3 = 0;

	uint32_t ctrl = reg_read(PL11X_REG_CTRL);

	/* reset video if already enabled */
	if (ctrl & CTRL_POWER) {
		ctrl &= ~CTRL_POWER;
		reg_write(PL11X_REG_CTRL, ctrl);
		_timer.msleep(100);
	}
	if (ctrl & CTRL_ENABLED) {
		ctrl &= ~CTRL_ENABLED;
		reg_write(PL11X_REG_CTRL, ctrl);
		_timer.msleep(100);
	}

	ctrl = CTRL_BGR | CTRL_ENABLED | CTRL_TFT | CTRL_VCOMP | CTRL_BPP_24;

	/* init color-lcd oscillator */
	sys_reg_write(SP810_REG_LOCK,    0xa05f);
	sys_reg_write(SP810_REG_OSCCLCD, 0x2c77);
	sys_reg_write(SP810_REG_LOCK,    0);

	/* init video timing */
	reg_write(PL11X_REG_TIMING0, tim0);
	reg_write(PL11X_REG_TIMING1, tim1);
	reg_write(PL11X_REG_TIMING2, tim2);
	reg_write(PL11X_REG_TIMING3, tim3);

	/* set framebuffer address and ctrl register */
	reg_write(PL11X_REG_UPBASE, _fb_dma.dma_addr());
	reg_write(PL11X_REG_LPBASE, 0);
	reg_write(PL11X_REG_IMSC,   0);
	reg_write(PL11X_REG_CTRL,   ctrl);
	_timer.msleep(100);

	/* power on */
	reg_write(PL11X_REG_CTRL,   ctrl | CTRL_POWER);
}


void Component::construct(Genode::Env &env)
{
	static Pl11x_driver::Main main(env);
}
