/*
 * \brief  Virtual Machine Monitor
 * \author Stefan Kalkowski
 * \date   2012-06-25
 */

/*
 * Copyright (C) 2008-2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <base/env.h>
#include <base/sleep.h>
#include <base/thread.h>
#include <drivers/board_base.h>
#include <drivers/trustzone.h>

/* QT gui */
#include <vmm_gui_session/connection.h>

/* local includes */
#include <vm.h>
#include <atag.h>
#include <m4if.h>
#include <framebuffer.h>
#include <input.h>

using namespace Genode;

enum {
	KERNEL_OFFSET    = 0x8000,
	MACH_TYPE_TABLET = 3011,
	MACH_TYPE_QSB    = 3273,
	BOARD_REV_TABLET = 0x53321,
};


static const char* cmdline_tablet =
	"console=ttymxc0,115200 androidboot.console=ttymxc0 lpj=4997120 \
video=mxcdi1fb:RGB666,XGA gpu_memory=64M";


namespace Vmm {
	class Vmm;
}


class Vmm::Vmm : public Thread<8192>
{
	private:

		enum Devices {
			FRAMEBUFFER,
			INPUT,
		};

		class Input_handler : public Thread<8192>
		{
			private:

				::Input::Session  *_input;
				::Input::Event    *_ev_buf;
				Signal_transmitter _transmitter;
				Timer::Connection  _timer;

			public:

				Input_handler(::Input::Session *input,
				              Signal_context_capability cap)
				: Thread<8192>("input_handler"),
				  _input(input),
				  _ev_buf(Genode::env()->rm_session()->attach(input->dataspace())),
				  _transmitter(cap) { start(); }

				void entry() {
					while (true) {
						if (!_input->is_pending())
							_timer.msleep(20);

						unsigned num_events = _input->flush();
						for (unsigned i = 0; i < num_events; i++) {
							::Input::Event *ev = &_ev_buf[i];
							if (ev->code() == ::Input::KEY_POWER ||
								ev->code() == ::Input::BTN_LEFT) {
								if (ev->type() == ::Input::Event::PRESS)
									_transmitter.submit();
							}
						}
					}
				}
		};


		Signal_receiver           _sig_rcv;
		Signal_context            _vm_context;
		Signal_context            _input_context;
		Signal_context            _play_bt_context;
		Signal_context            _stop_bt_context;
		Signal_context            _bomb_bt_context;
		Signal_context            _power_bt_context;
		Signal_context_capability _input_sig_cxt;
		Vmm_gui::Connection       _gui;
		Vm                       *_vm;
		Io_mem_connection         _m4if_io_mem;
		M4if                      _m4if;
		Nitpicker::Connection     _nitpicker;
		Nitpicker::View_client    _view;
		Framebuffer               _fb;
		Input                     _input;
		Input_handler             _input_handler;
		bool                      _foreground;
		bool                      _running;

		void _handle_hypervisor_call()
		{
			/* check device number*/
			switch (_vm->state()->r0) {
			case FRAMEBUFFER:
				_fb.handle(_vm->state());
				break;
			case INPUT:
				_input.handle(_vm->state());
				break;
			default:
				PERR("Unknown hypervisor call!");
				_vm->dump();
			};
		}

		bool _handle_data_abort()
		{
			_vm->dump();
			return false;
		}

		bool _handle_vm()
		{
			/* check exception reason */
			switch (_vm->state()->cpu_exception) {
			case Cpu_state::DATA_ABORT:
				if (!_handle_data_abort()) {
					PERR("Could not handle data-abort will exit!");
					return false;
				}
				break;
			case Cpu_state::SUPERVISOR_CALL:
				_handle_hypervisor_call();
				break;
			default:
				PERR("Curious exception occured");
				_vm->dump();
				return false;
			}
			return true;
		}

		Nitpicker::View_capability _buffer() {
			_nitpicker.buffer(::Framebuffer::Mode(SMALL_WIDTH, SMALL_HEIGHT,
			                                      ::Framebuffer::Mode::RGB565),
			                  false);
			return _nitpicker.create_view();
		}

	protected:

		void entry()
		{
			_vm->sig_handler(_sig_rcv.manage(&_vm_context));
			_gui.show_view(_view, SMALL_WIDTH, SMALL_HEIGHT);
			_gui.play_resume_sigh(_sig_rcv.manage(&_play_bt_context));
			_gui.stop_sigh(_sig_rcv.manage(&_stop_bt_context));
			_gui.bomb_sigh(_sig_rcv.manage(&_bomb_bt_context));
			_gui.power_sigh(_sig_rcv.manage(&_power_bt_context));
			_vm->start();
			_gui.set_state(_vm->state());

			while (true) {
				Signal s = _sig_rcv.wait_for_signal();
				if (s.context() == &_vm_context) {
					if (_handle_vm())
						_vm->run();
					else
						_gui.set_state(_vm->state());
				} else if (s.context() == &_input_context) {
					if (_foreground) {
						_input.background();
						_fb.background();
					} else {
						_fb.foreground();
						_input.foreground();
					}
					_foreground = !_foreground;
				} else if ((s.context() == &_play_bt_context))  {
					_running = !_running;
					if (!_running) {
						_vm->pause();
						_gui.set_state(_vm->state());
					} else
						_vm->run();
				} else if ((s.context() == &_stop_bt_context))  {
					if (_running)
						_vm->pause();
					_running = false;
					_vm->start();
					_gui.set_state(_vm->state());
				} else if ((s.context() == &_bomb_bt_context))  {
					if (_running)
						_vm->pause();
					/* real fuck up */
					_vm->state()->ip   = 0xc0041e64; /* bad_stuff addr  */
					_vm->state()->cpsr = 0x93;       /* supervisor mode */
					_vm->state()->r9   = Board_base::IPU_BASE;
					if (_running)
						_vm->run();
					else
						_gui.set_state(_vm->state());
				} else if ((s.context() == &_power_bt_context)) {
					_input.power_button();
				} else {
					PWRN("Invalid context");
					continue;
				}
			}
		};

	public:

		Vmm(Vm *vm)
		: Thread<8192>("vmm"),
		  _input_sig_cxt(_sig_rcv.manage(&_input_context)),
		  _vm(vm),
		  _m4if_io_mem(Board_base::M4IF_BASE, Board_base::M4IF_SIZE),
		  _m4if((addr_t)env()->rm_session()->attach(_m4if_io_mem.dataspace())),
		  _view(_buffer()),
		  _fb(vm, _nitpicker.framebuffer_session()),
		  _input(vm, _input_sig_cxt),
		  _input_handler(_nitpicker.input(), _input_sig_cxt),
		  _foreground(false),
		  _running(false)
		{
			_m4if.set_region0(Trustzone::SECURE_RAM_BASE,
			                  Trustzone::SECURE_RAM_SIZE);
		}
};


int main()
{
	static Vm vm("linux", "initrd.gz", cmdline_tablet,
	             Trustzone::NONSECURE_RAM_BASE, Trustzone::NONSECURE_RAM_SIZE,
	             KERNEL_OFFSET, MACH_TYPE_TABLET, BOARD_REV_TABLET);
	static Vmm::Vmm vmm(&vm);

	PINF("Start virtual machine ...");
	vmm.start();

	sleep_forever();
	return 0;
}
