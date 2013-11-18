/*
 * \brief  Virtual Machine Monitor i.mx53 specific input virtual device
 * \author Stefan Kalkowski
 * \date   2012-06-25
 */

/*
 * Copyright (C) 2008-2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _SRC__SERVER__VMM__IMX53__INPUT_H_
#define _SRC__SERVER__VMM__IMX53__INPUT_H_

/* Genode includes */
#include <base/signal.h>
#include <nitpicker_session/connection.h>
#include <nitpicker_view/client.h>
#include <timer_session/connection.h>
#include <input/event_queue.h>
#include <input/event.h>
#include <input/keycodes.h>

/* local includes */
#include <vm.h>
#include <framebuffer.h>

namespace Vmm {
	class Input;
}


class Vmm::Input : public Genode::Thread<8192>
{
	private:

		Vm                        *_vm;
		Nitpicker::Connection      _nitpicker;
		Nitpicker::View_capability _view_cap;
		Event_queue                _local_queue;
		::Input::Event            *_ev_buf;
		Genode::Signal_transmitter _sig_trans;
		Timer::Connection          _timer;

		enum Opcodes {
			GET_EVENT,
		};

		enum Type {
			INVALID,
			PRESS,
			RELEASE,
			MOTION
		};

	public:

		Input(Vm *vm, Genode::Signal_context_capability cap)
		: Genode::Thread<8192>("input_handler"),
		  _vm(vm),
		  _view_cap(_nitpicker.create_view()),
		  _ev_buf(Genode::env()->rm_session()->attach(_nitpicker.input()->dataspace())),
		  _sig_trans(cap)
		{
			_local_queue.enable();
			start();
		}


		void entry()
		{
			while (true) {
				if (!_nitpicker.input()->is_pending())
					_timer.msleep(10);

				unsigned num_events = _nitpicker.input()->flush();
				for (unsigned i = 0; i < num_events; i++) {
					::Input::Event *ev = &_ev_buf[i];
					if (ev->code() == ::Input::KEY_POWER) {
						if (ev->type() == ::Input::Event::PRESS)
							_sig_trans.submit();
					} else
						_local_queue.add(*ev);
				}
			}
		}


		void foreground() {
			using namespace Nitpicker;

			_view_cap = _nitpicker.create_view();
			View_client vc(_view_cap);
			vc.viewport(0, 0, VM_WIDTH, VM_HEIGHT, 0, 0, true);
			vc.stack(Nitpicker::View_capability(), true, true);
		}

		void background() { _nitpicker.destroy_view(_view_cap); }

		void power_button() {
			_local_queue.add(::Input::Event(::Input::Event::PRESS,
			                                ::Input::KEY_POWER, 0, 0, 0, 0));
			_local_queue.add(::Input::Event(::Input::Event::RELEASE,
			                                ::Input::KEY_POWER, 0, 0, 0, 0));
		}

		void handle(Vm_state * volatile state)
		{
			switch (state->r1) {
			case GET_EVENT:
				{
					state->r0 = INVALID;

					if (_local_queue.empty())
						return;

					::Input::Event ev = _local_queue.get();
					switch (ev.type()) {
					case ::Input::Event::PRESS:
						state->r0 = PRESS;
						state->r3 = ev.code();
						break;
					case ::Input::Event::RELEASE:
						state->r0 = RELEASE;
						state->r3 = ev.code();
						break;
					case ::Input::Event::MOTION:
						state->r0 = MOTION;
						break;
					default:
						return;
					}
					state->r1 = ev.ax();
					state->r2 = ev.ay();
					break;
				}
			default:
				PWRN("Unknown opcode!");
				_vm->dump();
			};
		}
};

#endif /* _SRC__SERVER__VMM__IMX53__INPUT_H_ */
