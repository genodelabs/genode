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
#include <dataspace/client.h>
#include <framebuffer_session/framebuffer_session.h>
#include <os/static_root.h>
#include <platform_session/connection.h>
#include <platform_device/client.h>
#include <timer_session/connection.h>


/***********************************************
 ** Implementation of the framebuffer service **
 ***********************************************/

namespace Framebuffer
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

	class Session_component;

	using namespace Genode;
	class Main;
}

class Framebuffer::Session_component :
	public Genode::Rpc_object<Framebuffer::Session>
{
	private:

		Ram_dataspace_capability _fb_ds_cap;
		addr_t                   _regs_base;
		addr_t                   _sys_regs_base;
		Timer::Connection        _timer;

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

		void sys_reg_write(unsigned reg, long value) {
			*(volatile long *)(_sys_regs_base + sizeof(long)*reg) = value; }

		long sys_reg_read(unsigned reg) {
			return *(volatile long *)(_sys_regs_base + sizeof(long)*reg); }

		void reg_write(unsigned reg, long value) {
			*(volatile long *)(_regs_base + sizeof(long)*reg) = value; }

		long reg_read(unsigned reg) {
			return *(volatile long *)(_regs_base + sizeof(long)*reg); }

	public:

		/**
		 * Constructor
		 */
		Session_component(Env                    & env,
		                  void                   * regs_base,
		                  void                   * sys_regs_base,
		                  Ram_dataspace_capability fb_ds_cap,
		                  Genode::addr_t           fb_ds_bus_addr)
		: _fb_ds_cap(fb_ds_cap),
		  _regs_base((addr_t)regs_base),
		  _sys_regs_base((addr_t)sys_regs_base),
		  _timer(env)
		{
			using namespace Genode;

			uint32_t tim0 = (SCR_WIDTH/16 - 1) << 2
			              | (HSYNC_LEN    - 1) << 8
			              | (RIGHT_MARGIN - 1) << 16
			              | (LEFT_MARGIN  - 1) << 24;
			uint32_t tim1 = (SCR_HEIGHT - 1)
			              | (VSYNC_LEN - 1)    << 10
			              |  LOWER_MARGIN      << 16
			              |  UPPER_MARGIN      << 24;
			uint32_t tim2 = ((SCR_WIDTH - 1)   << 16)
			              | CLCDC_IVS | CLCDC_IHS | CLCDC_BCD;
			uint32_t tim3 = 0;
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
			reg_write(PL11X_REG_UPBASE, fb_ds_bus_addr);
			reg_write(PL11X_REG_LPBASE, 0);
			reg_write(PL11X_REG_IMSC,   0);
			reg_write(PL11X_REG_CTRL,   ctrl);
			_timer.msleep(100);

			/* power on */
            reg_write(PL11X_REG_CTRL,   ctrl | CTRL_POWER);
		}

		Genode::Dataspace_capability dataspace() override { return _fb_ds_cap; }

		Mode mode() const override { return Mode { .area { SCR_WIDTH, SCR_HEIGHT } }; }

		void mode_sigh(Genode::Signal_context_capability) override { }

		void sync_sigh(Genode::Signal_context_capability sigh) override
		{
			_timer.sigh(sigh);
			_timer.trigger_periodic(10*1000);
		}

		void refresh(int, int, int, int) override { }
};


struct Framebuffer::Main
{
	Env                    & _env;
	Platform::Connection     _platform   { _env };
	Platform::Device_client  _pl11x_dev  {
		_platform.device_by_property("compatible", "arm,pl111") };
	Platform::Device_client  _sp810_dev  {
		_platform.device_by_property("compatible", "arm,sp810") };
	Attached_dataspace       _lcd_io_mem { _env.rm(),
	                                       _pl11x_dev.io_mem_dataspace() };
	Attached_dataspace       _sys_mem    { _env.rm(),
	                                       _sp810_dev.io_mem_dataspace() };
	Ram_dataspace_capability _fb_ds_cap  {
		_platform.alloc_dma_buffer(FRAMEBUFFER_SIZE) };

	Session_component _fb_session { _env,
	                                _lcd_io_mem.local_addr<void*>(),
	                                _sys_mem.local_addr<void*>(),
	                                _fb_ds_cap,
	                                _platform.bus_addr_dma_buffer(_fb_ds_cap) };

	Static_root<Session> _fb_root { _env.ep().manage(_fb_session) };

	Main(Env &env) : _env(env)
	{
		log("--- pl11x framebuffer driver ---\n");

		/* announce service */
		_env.parent().announce(env.ep().manage(_fb_root));
	}

	private:

		/*
		 * Noncopyable
		 */
		Main(Main const &);
		Main &operator = (Main const &);
};


void Component::construct(Genode::Env &env)
{
	static Framebuffer::Main main(env);
}
