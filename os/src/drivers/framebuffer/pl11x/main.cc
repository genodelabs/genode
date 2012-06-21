/*
 * \brief  PL11x frame-buffer driver
 * \author Norman Feske
 * \author Stefan Kalkowski
 * \date   2010-02-17
 */

/*
 * Copyright (C) 2010-2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <base/printf.h>
#include <base/sleep.h>
#include <base/rpc_server.h>
#include <io_mem_session/connection.h>
#include <cap_session/connection.h>
#include <dataspace/client.h>
#include <timer_session/connection.h>
#include <framebuffer_session/framebuffer_session.h>
#include <root/component.h>
#include <os/ring_buffer.h>

/* device configuration */
#include <pl11x_defs.h>
#include <sp810_defs.h>
#include <video_memory.h>


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

		BYTES_PER_PIXEL  = 2,
		FRAMEBUFFER_SIZE = SCR_WIDTH*SCR_HEIGHT*BYTES_PER_PIXEL,
	};

	class Session_component : public Genode::Rpc_object<Session>
	{
		private:

			Genode::Dataspace_capability _fb_ds_cap;
			Genode::Dataspace_client     _fb_ds;
			Genode::addr_t               _regs_base;
			Genode::addr_t               _sys_regs_base;
			Timer::Connection            _timer;

			enum {
				/**
				 * Bit definitions of the lcd control register
				 */
				CTRL_ENABLED   = 1 <<  0,
				CTRL_BPP16     = 4 <<  1,
				CTRL_BPP16_565 = 6 <<  1,
				CTRL_TFT       = 1 <<  5,
				CTRL_BGR       = 1 <<  8,
				CTRL_POWER     = 1 << 11,
				CTRL_VCOMP     = 1 << 12,

				/**
				 * Bit definitions for CLCDC timing.
				 */
				CLCDC_IVS     = 1 << 11,
				CLCDC_IHS     = 1 << 12,
				CLCDC_BCD     = 1 << 26,
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
			Session_component(void *regs_base, void *sys_regs_base,
			                  Genode::Dataspace_capability fb_ds_cap)
			: _fb_ds_cap(fb_ds_cap), _fb_ds(_fb_ds_cap),
			  _regs_base((Genode::addr_t)regs_base),
			  _sys_regs_base((Genode::addr_t)sys_regs_base)
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

				ctrl = CTRL_BGR | CTRL_ENABLED | CTRL_TFT | CTRL_VCOMP | CTRL_BPP16_565;

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
				reg_write(PL11X_REG_UPBASE, _fb_ds.phys_addr());
				reg_write(PL11X_REG_LPBASE, 0);
				reg_write(PL11X_REG_IMSC,   0);
				reg_write(PL11X_REG_CTRL,   ctrl);
				_timer.msleep(100);

				/* power on */
                reg_write(PL11X_REG_CTRL,   ctrl | CTRL_POWER);
			}

			Genode::Dataspace_capability dataspace() { return _fb_ds_cap; }

			void release() { }

			Mode mode() const { return Mode(SCR_WIDTH, SCR_HEIGHT, Mode::RGB565); }

			void mode_sigh(Genode::Signal_context_capability) { }

			void refresh(int x, int y, int w, int h) { }
	};


	class Root : public Genode::Root_component<Session_component>
	{
		private:

			void                        *_lcd_regs_base;
			void                        *_sys_regs_base;
			Genode::Dataspace_capability _fb_ds_cap;

		protected:

			Session_component *_create_session(const char *args) {
				return new (md_alloc()) Session_component(_lcd_regs_base,
				                                          _sys_regs_base,
				                                          _fb_ds_cap); }

		public:

			Root(Genode::Rpc_entrypoint *session_ep, Genode::Allocator *md_alloc,
			     void *lcd_regs_base, void *sys_regs_base,
			     Genode::Dataspace_capability fb_ds_cap)
			: Genode::Root_component<Session_component>(session_ep, md_alloc),
			  _lcd_regs_base(lcd_regs_base), _sys_regs_base(sys_regs_base),
			  _fb_ds_cap(fb_ds_cap) { }
	};
}


using namespace Genode;

int main(int, char **)
{
	printf("--- pl11x framebuffer driver ---\n");

	/* locally map LCD control registers */
	Io_mem_connection lcd_io_mem(PL11X_LCD_PHYS, PL11X_LCD_SIZE);
	void *lcd_base = env()->rm_session()->attach(lcd_io_mem.dataspace());

	/* locally map system control registers */
	Io_mem_connection sys_mem(SP810_PHYS, SP810_SIZE);
	void *sys_base = env()->rm_session()->attach(sys_mem.dataspace());

	enum { STACK_SIZE = 4096 };
	static Cap_connection cap;
	static Rpc_entrypoint ep(&cap, STACK_SIZE, "fb_ep");

	Dataspace_capability fb_ds_cap =
		Framebuffer::alloc_video_memory(Framebuffer::FRAMEBUFFER_SIZE);

	/*
	 * Let the entry point serve the framebuffer and input root interfaces
	 */
	static Framebuffer::Root fb_root(&ep, env()->heap(), lcd_base, sys_base, fb_ds_cap);
	env()->parent()->announce(ep.manage(&fb_root));

	sleep_forever();

	return 0;
}
