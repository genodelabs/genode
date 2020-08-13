/*
 * \brief  PS/2 mouse protocol handler
 * \author Norman Feske
 * \date   2007-10-09
 */

/*
 * Copyright (C) 2007-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _DRIVERS__INPUT__SPEC__PS2__PS2_MOUSE_H_
#define _DRIVERS__INPUT__SPEC__PS2__PS2_MOUSE_H_

#include <base/log.h>
#include <input/event_queue.h>
#include <input/keycodes.h>
#include <timer_session/connection.h>

#include "input_driver.h"

namespace Ps2 { class Mouse; }

class Ps2::Mouse : public Input_driver
{
	enum Command
	{
		CMD_GET_ID         = 0xf2,
		CMD_SET_RATE       = 0xf3,
		CMD_ENABLE_STREAM  = 0xf4,
		CMD_DISABLE_STREAM = 0xf5,
		CMD_SET_DEFAULTS   = 0xf6,
		CMD_RESET          = 0xff,
	};

	enum Return
	{
		RET_ACK      = 0xfa,
		RET_NAK      = 0xfe,
		RET_ERROR    = 0xff,
	};

	enum Flag
	{
		FLAG_BTN_LEFT   = 0x01,
		FLAG_BTN_RIGHT  = 0x02,
		FLAG_BTN_MIDDLE = 0x04,
		FLAG_X_SIGN     = 0x10,
		FLAG_Y_SIGN     = 0x20,
		FLAG_X_OVER     = 0x40,
		FLAG_Y_OVER     = 0x80,
	};

	enum {
		LEFT   = 0,
		RIGHT  = 1,
		MIDDLE = 2,
		SIDE   = 3,
		EXTRA  = 4,
		NUM_BUTTONS,
	};

	enum {
		PS2_PACKET_LEN   = 3,
		IMPS2_PACKET_LEN = 4,
		EXPS2_PACKET_LEN = 4,
		MAX_PACKET_LEN   = 4,
	};

	enum Type { PS2, IMPS2, EXPS2 };

	private:

		Serial_interface   &_aux;
		Type                _type { PS2 };
		Timer::Connection  &_timer;
		Verbose      const &_verbose;
		bool                _button_state[NUM_BUTTONS];

		unsigned char _packet[MAX_PACKET_LEN];
		int           _packet_len { PS2_PACKET_LEN };
		int           _packet_idx = 0;

		/**
		 * Probe for extended ImPS/2 mouse (IntelliMouse)
		 */
		bool _probe_imps2()
		{
			/* send magic rate setting combination */
			const unsigned char magic[] = { CMD_SET_RATE, 200, CMD_SET_RATE, 100, CMD_SET_RATE, 80 };

			for (unsigned i = 0; i < sizeof(magic); ++i) {
				_aux.write(magic[i]);
				if (_aux.read() != RET_ACK) return false;
			}

			/* check device ID */
			unsigned char id;
			_aux.write(CMD_GET_ID);
			if (_aux.read() != RET_ACK) return false;
			id = _aux.read();

			return id == 3;
		}

		/**
		 * Probe for extended ExPS/2 mouse (IntelliMouse Explorer)
		 */
		bool _probe_exps2()
		{
			/* send magic rate setting combination */
			const unsigned char magic[] = { CMD_SET_RATE, 200, CMD_SET_RATE, 200, CMD_SET_RATE, 80 };

			for (unsigned i = 0; i < sizeof(magic); ++i) {
				_aux.write(magic[i]);
				if (_aux.read() != RET_ACK) return false;
			}

			/* check device ID */
			unsigned char id;
			_aux.write(CMD_GET_ID);
			if (_aux.read() != RET_ACK) return false;
			id = _aux.read();

			return id == 4;
		}

		bool _wait_for_data_ready()
		{
			/* poll TIMEOUT_MS for reset results each SLEEP_MS */
			enum { TIMEOUT_MS = 700, SLEEP_MS = 10 };
			unsigned timeout_ms = 0;
			do {
				_timer.msleep(SLEEP_MS);
				timeout_ms += SLEEP_MS;
			} while (!_aux.data_read_ready() && timeout_ms < TIMEOUT_MS);

			return _aux.data_read_ready();
		}

	public:

		Mouse(Serial_interface &aux, Timer::Connection &timer, Verbose const &verbose)
		:
			_aux(aux), _timer(timer), _verbose(verbose)
		{
			for (unsigned i = 0; i < NUM_BUTTONS; ++i)
				_button_state[i] = false;
			reset();
		}

		void reset()
		{
			_aux.write(CMD_RESET);

			if (!_wait_for_data_ready()) {
				Genode::warning("could not reset mouse (no response)");
				return;
			}

			if (_aux.read() != RET_ACK) {
				Genode::warning("could not reset mouse (missing ack)");
				return;
			}

			if (!_wait_for_data_ready()) {
				Genode::warning("could not reset mouse (no response)");
				return;
			}

			if (_aux.read() != 0xaa)
				Genode::warning("could not reset mouse (unexpected response)");
			if (_aux.read() != 0x00)
				Genode::warning("could not reset mouse (unexpected secondary response)");

			_aux.write(CMD_ENABLE_STREAM);
			if (_aux.read() != RET_ACK)
				Genode::warning("could not enable stream");

			/*
			 * Give the hardware some time to settle before probing extended
			 * mouse versions. Otherwise, current Lenovo trackpoints (X260,
			 * T470) stop working.
			 */
			_timer.msleep(5);

			/* probe for protocol extensions */
			if (_probe_exps2()) {
				_type       = EXPS2;
				_packet_len = EXPS2_PACKET_LEN;
				Genode::log("detected ExPS/2 mouse - activating scroll-wheel and 5-button support");
			} else if (_probe_imps2()) {
				_type       = IMPS2;
				_packet_len = IMPS2_PACKET_LEN;
				Genode::log("detected ImPS/2 mouse - activating scroll-wheel support");
			}

			/* set sane sample rate */
			_aux.write(CMD_SET_RATE);
			if (_aux.read() == RET_ACK) {
				_aux.write(100);
				_aux.read();
			}
		}


		/****************************
		 ** Input driver interface **
		 ****************************/

		void handle_event(Event::Session_client::Batch &batch) override
		{
			_packet[_packet_idx++] = _aux.read();
			if (_packet_idx < _packet_len)
				return;

			/* decode packet and feed event queue */
			int rel_x = _packet[1], rel_y = _packet[2];

			/* shortcut for packet header */
			int ph = _packet[0];

			/* sign extend motion values */
			rel_x |= (ph & FLAG_X_SIGN) ? ~0xff : 0;
			rel_y |= (ph & FLAG_Y_SIGN) ? ~0xff : 0;

			/* discard motion values on overflow */
			rel_x &= (ph & FLAG_X_OVER) ? 0 : ~0;
			rel_y &= (ph & FLAG_Y_OVER) ? 0 : ~0;

			/* generate motion event */
			if (rel_x || rel_y) {

				/* mirror y axis to make the movement correspond to screen coordinates */
				rel_y = -rel_y;

				if (_verbose.mouse)
					Genode::log("post MOTION, rel_x=", rel_x, ", rel_y=", rel_y);

				batch.submit(Input::Relative_motion{rel_x, rel_y});
			}

			/* generate wheel event */
			int rel_z = 0;
			if (_type == IMPS2)
				rel_z = (signed char) _packet[3];
			if (_type == EXPS2) {
				rel_z = _packet[3] & 0xf;
				/* sign extend value */
				rel_z |= (rel_z & 0x8) ? ~0x0f : 0;
			}
			if (rel_z) {
				/* mirror y axis to make "scroll up" generate positive values */
				rel_z = -rel_z;

				if (_verbose.mouse)
					Genode::log("post WHEEL, rel_z=", rel_z);

				batch.submit(Input::Wheel{0, rel_z});
			}

			/**
			 * Generate mouse button event on state changes
			 *
			 * Depending on the old and new state, this function
			 * posts press or release events for the mouse buttons
			 * to the event queue. Note that the old state value
			 * gets set to the new state.
			 */
			auto button_event = [&] (bool *old_state, bool new_state, int key_code)
			{
				if (*old_state == new_state) return;

				if (_verbose.mouse)
					Genode::log("post ", new_state ? "PRESS" : "RELEASE", ", "
					            "key_code=", key_code);

				if (new_state)
					batch.submit(Input::Press{Input::Keycode(key_code)});
				else
					batch.submit(Input::Release{Input::Keycode(key_code)});

				*old_state = new_state;
			};

			/* detect changes of mouse-button state and post corresponding events */
			button_event(&_button_state[LEFT],   ph & FLAG_BTN_LEFT,   Input::BTN_LEFT);
			button_event(&_button_state[RIGHT],  ph & FLAG_BTN_RIGHT,  Input::BTN_RIGHT);
			button_event(&_button_state[MIDDLE], ph & FLAG_BTN_MIDDLE, Input::BTN_MIDDLE);

			/* post extra button events */
			if (_type == EXPS2) {
				button_event(&_button_state[SIDE],  _packet[3] & 0x10, Input::BTN_SIDE);
				button_event(&_button_state[EXTRA], _packet[3] & 0x20, Input::BTN_EXTRA);
			}

			/* start new packet */
			_packet_idx = 0;
		}

		bool event_pending() const override { return _aux.data_read_ready(); }
};

#endif /* _DRIVERS__INPUT__SPEC__PS2__PS2_MOUSE_H_ */
