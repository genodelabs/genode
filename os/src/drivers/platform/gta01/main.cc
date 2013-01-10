/*
 * \brief  Simplistic frame-buffer and input driver for the GTA01 chip set
 * \author Norman Feske
 * \date   2009-11-20
 *
 * This driver is a proof-of-concept driver for the touch screen and frame
 * buffer of the GTA01 chip set as provided by Qemu-neo1973. Note that the
 * driver is not tested on real hardware. The touch-screen calibration is
 * statically configured to the ADC values reported by Qemu. For simplicity,
 * the driver polls for user input rather than using interrupts. The frame-
 * buffer driver relies on the screen initialization as done by the boot
 * loader (u-boot), which is expected to set up a 480x640 rgb565 screen mode.
 */

/*
 * Copyright (C) 2009-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#include <base/printf.h>
#include <base/rpc_server.h>
#include <io_mem_session/connection.h>
#include <cap_session/connection.h>
#include <dataspace/client.h>
#include <framebuffer_session/framebuffer_session.h>
#include <timer_session/connection.h>
#include <root/component.h>
#include <input/component.h>
#include <input/keycodes.h>
#include <input/event.h>
#include <os/ring_buffer.h>

const bool config_verbose = false;


/*****************************************
 ** Implementation of the input service **
 *****************************************/

class Event_queue : public Ring_buffer<Input::Event, 256> { };

static Event_queue ev_queue;

namespace Input {
	void event_handling(bool enable) { }
	bool event_pending() { return !ev_queue.empty(); }
	Event get_event() { return ev_queue.get(); }
}


/***********************************************
 ** Implementation of the framebuffer service **
 ***********************************************/

namespace Framebuffer
{
	enum {
		SCR_WIDTH  = 480,
		SCR_HEIGHT = 640,
	};

	class Session_component : public Genode::Rpc_object<Session>
	{
		private:

			Genode::Dataspace_capability _fb_ds_cap;
			Genode::Dataspace_client     _fb_ds;

			enum {
				BYTES_PER_PIXEL  = 2,
				FRAMEBUFFER_SIZE = SCR_WIDTH*SCR_HEIGHT*BYTES_PER_PIXEL,

				S3C_LCD_SCR_ADDR = 0x14, /* relative to lcd reg base */
			};

		public:

			/**
			 * Constructor
			 */
			Session_component(void *lcd_regs_base) :
				_fb_ds_cap(Genode::env()->ram_session()->alloc(FRAMEBUFFER_SIZE)),
				_fb_ds(_fb_ds_cap)
			{
				using namespace Genode;

				/* init video */
				Genode::addr_t addr = _fb_ds.phys_addr();
				*(volatile long *)((addr_t)lcd_regs_base + S3C_LCD_SCR_ADDR) = addr >> 1;
			}

			Genode::Dataspace_capability dataspace() { return _fb_ds_cap; }

			void info(int *out_w, int *out_h, Mode *out_mode)
			{
				*out_w    = SCR_WIDTH;
				*out_h    = SCR_HEIGHT;
				*out_mode = RGB565;
			}

			void refresh(int x, int y, int w, int h) { }
	};


	class Root : public Genode::Root_component<Session_component>
	{
		private:

			void *_lcd_regs_base;

		protected:

			Session_component *_create_session(const char *args) {
				return new (md_alloc()) Session_component(_lcd_regs_base); }

		public:

			Root(Genode::Rpc_entrypoint *session_ep,
			     Genode::Allocator *md_alloc, void *lcd_regs_base)
			:
				Genode::Root_component<Session_component>(session_ep, md_alloc),
				_lcd_regs_base(lcd_regs_base) { }
	};
}


class S3c_adc
{
	private:

		long _base;

		/*
		 * ADC register offsets
		 */
		enum {
			S3C_ADC_CON  = 0x0,
			S3C_ADC_TSC  = 0x04,
			S3C_ADC_DAT0 = 0x0c,
			S3C_ADC_DAT1 = 0x10,
		};

		long _reg_read(long reg)
		{
			return *(volatile long *)(_base + reg);
		}

		void _reg_write(long reg, long value)
		{
			*(volatile long *)(_base + reg) = value;
		}

		long _dat0() { return _reg_read(S3C_ADC_DAT0); }
		long _dat1() { return _reg_read(S3C_ADC_DAT1); }
		long _con()  { return _reg_read(S3C_ADC_CON); }

		void _tsc(long tsc) { _reg_write(S3C_ADC_TSC, tsc); }
		void _con(long con) { _reg_write(S3C_ADC_CON, con); }

	public:

		/**
		 * Constructor
		 *
		 * \param base  local base address of ADC registers
		 */
		S3c_adc(void *base)
		: _base((long)base)
		{
			enum {
				ADC_TSC_XY_PST_NOP         = 3 << 0,
				ADC_TSC_AUTO_PST           = 1 << 2,
				ADC_TSC_PULL_UP_DISABLE    = 1 << 3,
				ADC_TSC_XP_SEN_AIN7        = 1 << 4,
				ADC_TSC_XM_SEN_EXT_VOLTAGE = 1 << 5,
				ADC_TSC_YP_SEN_AIN5        = 1 << 6,
				ADC_TSC_YM_SEN_GND         = 1 << 7,
			};

			_tsc(ADC_TSC_AUTO_PST
			   | ADC_TSC_PULL_UP_DISABLE
			   | ADC_TSC_XP_SEN_AIN7
			   | ADC_TSC_YP_SEN_AIN5
			   | ADC_TSC_YM_SEN_GND);

			enum {
				ADC_CON_START_ENABLE = 1 << 0,
				ADC_CON_STDBM = 1 << 2,
			};

			long v = _con();
			v &= ~(ADC_CON_START_ENABLE | ADC_CON_STDBM);
			v |= ADC_CON_START_ENABLE;
			_con(v);
		}

		/**
		 * Return true if pen is pressed
		 */
		bool pen_down()
		{
			enum { PEN_DOWN = 1 << 15 };
			return !(_dat0() & PEN_DOWN);
		}

		/**
		 * Return x position of pen, in screen coordinates
		 */
		int x()
		{
			/*
			 *      x       ADC_LEFT - adc_value
			 *  --------- = --------------------
			 *  SCR_WIDTH   ADC_LEFT - ADC_RIGHT
			 */
			enum { ADC_LEFT = 0x4a, ADC_RIGHT = 0x3a9 };
			int v = _dat1() & 0x3fff;
			return ((ADC_LEFT - v)*Framebuffer::SCR_WIDTH)/(ADC_LEFT - ADC_RIGHT);
		}

		/**
		 * Return y position of pen, in screen coordinates
		 */
		int y()
		{
			/*
			 *      y        ADC_TOP - adc_value
			 *  ---------- = --------------------
			 *  SCR_HEIGHT   ADC_TOP - ADC_BOTTOM
			 */
			enum { ADC_BOTTOM = 0xc3d, ADC_TOP = 0xfa7 };
			int v = _dat0() & 0x3fff;
			return ((ADC_TOP - v)*Framebuffer::SCR_HEIGHT)/(ADC_TOP - ADC_BOTTOM);
		}
};


using namespace Genode;

int main(int, char **)
{
	printf("--- gta01 driver ---\n");

	/* locally map LCD control registers */
	enum { S3C_LCD_PHYS = 0x4d000000, S3C_LCD_SIZE = 0x01000000, };
	Io_mem_connection lcd_io_mem(S3C_LCD_PHYS, S3C_LCD_SIZE);
	void *lcd_base = env()->rm_session()->attach(lcd_io_mem.dataspace());

	/* locally map ADC registers (touchscreen is connected via ADC) */
	enum { S3C_ADC_PHYS = 0x58000000, S3C_ADC_SIZE = 0x1000 };
	Io_mem_connection adc_io_mem(S3C_ADC_PHYS, S3C_ADC_SIZE);
	void *adc_base = env()->rm_session()->attach(adc_io_mem.dataspace());

	enum { STACK_SIZE = 4096 };
	static Cap_connection cap;
	static Rpc_entrypoint ep(&cap, STACK_SIZE, "gta01_ep");

	/*
	 * Let the entry point serve the framebuffer and input root interfaces
	 */
	static Framebuffer::Root fb_root(&ep, env()->heap(), lcd_base);
	env()->parent()->announce(ep.manage(&fb_root));

	static Input::Root input_root(&ep, env()->heap());
	env()->parent()->announce(ep.manage(&input_root));

	static Timer::Connection timer;
	static S3c_adc s3c_adc(adc_base);

	/*
	 * Poll for touch-screen input
	 */
	bool old_pen_down = s3c_adc.pen_down();
	int old_x = s3c_adc.x(), old_y = s3c_adc.y();
	for (;;) {
		for (int i = 0; i < 10; i++)
			timer.msleep(10);

		int  new_x = s3c_adc.x(), new_y = s3c_adc.y();
		bool new_pen_down = s3c_adc.pen_down();

		/* generate motion event */
		if (new_x != old_x || new_y != old_y) {
			if (config_verbose)
				printf("x=%d, y=%d\n", new_x, new_y);
			Input::Event ev(Input::Event::MOTION,
			                0, new_x, new_y, 0, 0);
			ev_queue.add(ev);
		}

		/* generate button press/release event */
		if (new_pen_down != old_pen_down) {
			if (config_verbose)
				printf("pen=%d -> %d\n", old_pen_down, new_pen_down);
			Input::Event ev(new_pen_down ? Input::Event::PRESS : Input::Event::RELEASE,
			                Input::BTN_LEFT, 0, 0, 0, 0);
			ev_queue.add(ev);
		}

		old_x = new_x, old_y = new_y;
		old_pen_down = new_pen_down;
	}
	return 0;
}
