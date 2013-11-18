/*
 * \brief  Virtual Machine Monitor i.mx53 specific framebuffer virtual device
 * \author Stefan Kalkowski
 * \date   2012-06-25
 */

/*
 * Copyright (C) 2008-2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _SRC__SERVER__VMM__IMX53__FRAMEBUFFER_H_
#define _SRC__SERVER__VMM__IMX53__FRAMEBUFFER_H_

/* Genode includes */
#include <timer_session/connection.h>
#include <framebuffer_session/client.h>
#include <imx_framebuffer_session/connection.h>

/* local includes */
#include <vm.h>

namespace Vmm {
	class Framebuffer;

	enum Resolution {
		VM_WIDTH     = 1024,
		VM_HEIGHT    = 752,
		SMALL_WIDTH  = 256,
		SMALL_HEIGHT = 188,
	};
};

class Vmm::Framebuffer : public Genode::Thread<8192>
{
	private:

		enum {
			FOREGROUND = 16,
			BACKGROUND = 768,
			STEPS      = 100,
			TICK_MS    = 10,
			QUANTUM_PX = 162,
			QUANTUM_AL = 61,
		};

		Vm                            *_vm;
		::Framebuffer::Session_client  _fb;
		::Framebuffer::Imx_connection  _overlay;
		Genode::addr_t                 _fb_phys_base;
		Genode::addr_t                 _fb_small_base;
		int                            _offset;
		int                            _alpha;
		Timer::Connection              _timer;
		Genode::Lock                   _lock;
		bool                           _initialized;

		enum Opcodes {
			BASE,
		};

		void _blit()
		{
			try {
				Genode::uint16_t *src = (Genode::uint16_t*)_vm->ram()->va(_fb_phys_base);
				Genode::uint16_t *dst = (Genode::uint16_t*)_fb_small_base;
				for (unsigned line = 0; line < SMALL_HEIGHT;
					 line++, src+=VM_WIDTH*3)
					for (unsigned px = 0; px < SMALL_WIDTH; px++, dst++, src+=4)
						*dst = *src;
				_fb.refresh(0, 0, SMALL_WIDTH, SMALL_HEIGHT);
			} catch(Ram::Invalid_addr) { }
		}

		void _halt() { _lock.lock();   }
		void _run()  { _lock.unlock(); }

	public:

		Framebuffer(Vm *vm, ::Framebuffer::Session_capability cap)
		: Genode::Thread<8192>("blitter"),
		  _vm(vm),
		  _fb(cap),
		  _fb_phys_base(0),
		  _fb_small_base(Genode::env()->rm_session()->attach(_fb.dataspace())),
		  _offset(BACKGROUND),
		  _alpha(255),
		  _lock(Genode::Lock::UNLOCKED),
		  _initialized(false) { start(); }


		void entry()
		{
			Timer::Connection timer;

			while (true)
			{
				Genode::Lock::Guard guard(_lock);
				_blit();
				timer.msleep(25);
			}
		}


		void handle(Vm_state *state)
		{
			switch (state->r1) {
			case BASE:
				{
					_fb_phys_base = state->r2 ? state->r2 : _fb_phys_base;
					_overlay.overlay(_fb_phys_base, 0, _offset, _alpha);
					if (!_initialized) {
						_run();
						_initialized = true;
					}
					break;
				}
			default:
				PWRN("Unknown opcode!");
				_vm->dump();
			};
		}

		void foreground()
		{
			for (int i = STEPS-1; i >= 0; i--) {
				_timer.msleep(TICK_MS);
				_offset -= i*QUANTUM_PX / 1000;
				_alpha  -= i*QUANTUM_AL / 1000;
				_overlay.overlay(_fb_phys_base, 0, _offset, _alpha);
			}

			_offset = FOREGROUND;
			_alpha  = 0;
			_halt();
		}

		void background()
		{
			_run();
			for (int i = 0; i < STEPS; i++) {
				_timer.msleep(TICK_MS);
				_offset += i*QUANTUM_PX / 1000;
				_alpha  += i*QUANTUM_AL / 1000;
				_overlay.overlay(_fb_phys_base, 0, _offset, _alpha);
			}

			_offset = BACKGROUND;
			_alpha  = 255;
		}
};

#endif /* _SRC__SERVER__VMM__IMX53__FRAMEBUFFER_H_ */
