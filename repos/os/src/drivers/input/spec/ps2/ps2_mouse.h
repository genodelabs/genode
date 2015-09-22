/*
 * \brief  PS/2 mouse protocol handler
 * \author Norman Feske
 * \date   2007-10-09
 */

/*
 * Copyright (C) 2007-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _DRIVERS__INPUT__SPEC__PS2__PS2_MOUSE_H_
#define _DRIVERS__INPUT__SPEC__PS2__PS2_MOUSE_H_

#include <base/printf.h>
#include <input/event_queue.h>
#include <input/keycodes.h>

#include "input_driver.h"

class Ps2_mouse : public Input_driver
{
	enum Command
	{
		CMD_GET_ID         = 0xf2,
		CMD_SET_RATE       = 0xf3,
		CMD_ENABLE_STREAM  = 0xf4,
		CMD_DISABLE_STREAM = 0xf5,
		CMD_SET_DEFAULTS   = 0xf6,
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

		static const bool verbose = false;

		Serial_interface   &_aux;
		Input::Event_queue &_ev_queue;

		Type                _type;

		bool                _button_state[NUM_BUTTONS];

		unsigned char _packet[MAX_PACKET_LEN];
		int           _packet_len;
		int           _packet_idx;

		void _check_for_event_queue_overflow()
		{
			if (_ev_queue.avail_capacity())
				return;

			PWRN("event queue overflow - dropping events");
			_ev_queue.reset();
		}

		/**
		 * Generate mouse button event on state changes
		 *
		 * Depending on the old and new state, this function
		 * posts press or release events for the mouse buttons
		 * to the event queue. Note that the old state value
		 * gets set to the new state.
		 */
		void _button_event(bool *old_state, bool new_state, int key_code)
		{
			if (*old_state == new_state) return;

			if (verbose)
				Genode::printf("post %s, key_code = %d\n", new_state ? "PRESS" : "RELEASE", key_code);

			_check_for_event_queue_overflow();

			_ev_queue.add(Input::Event(new_state ? Input::Event::PRESS
			                                     : Input::Event::RELEASE,
			                           key_code, 0, 0, 0, 0));
			*old_state = new_state;
		}

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

	public:

		Ps2_mouse(Serial_interface &aux, Input::Event_queue &ev_queue)
		:
			_aux(aux),
			_ev_queue(ev_queue), _type(PS2),
			_packet_len(PS2_PACKET_LEN), _packet_idx(0)
		{
			for (unsigned i = 0; i < NUM_BUTTONS; ++i)
				_button_state[i] = false;
			reset();
		}

		void reset()
		{
			_aux.write(CMD_SET_DEFAULTS);
			if (_aux.read() != RET_ACK)
				PWRN("Could not set defaults");

			_aux.write(CMD_ENABLE_STREAM);
			if (_aux.read() != RET_ACK)
				PWRN("Could not enable stream");

			/* probe for protocol extensions */
			if (_probe_exps2()) {
				_type       = EXPS2;
				_packet_len = EXPS2_PACKET_LEN;
				Genode::printf("Detected ExPS/2 mouse - activating scroll-wheel and 5-button support.\n");
			} else if (_probe_imps2()) {
				_type       = IMPS2;
				_packet_len = IMPS2_PACKET_LEN;
				Genode::printf("Detected ImPS/2 mouse - activating scroll-wheel support.\n");
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

		void handle_event()
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

				if (verbose)
					Genode::printf("post MOTION, rel_x = %d, rel_y = %d\n", rel_x, rel_y);

				_check_for_event_queue_overflow();

				_ev_queue.add(Input::Event(Input::Event::MOTION,
				                           0, 0, 0, rel_x, rel_y));
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

				if (verbose)
					Genode::printf("post WHEEL, rel_z = %d\n", rel_z);

				_check_for_event_queue_overflow();

				_ev_queue.add(Input::Event(Input::Event::WHEEL,
				                           0, 0, 0, 0, rel_z));
			}

			/* detect changes of mouse-button state and post corresponding events */
			_button_event(&_button_state[LEFT],   ph & FLAG_BTN_LEFT,   Input::BTN_LEFT);
			_button_event(&_button_state[RIGHT],  ph & FLAG_BTN_RIGHT,  Input::BTN_RIGHT);
			_button_event(&_button_state[MIDDLE], ph & FLAG_BTN_MIDDLE, Input::BTN_MIDDLE);

			/* post extra button events */
			if (_type == EXPS2) {
				_button_event(&_button_state[SIDE],  _packet[3] & 0x10, Input::BTN_SIDE);
				_button_event(&_button_state[EXTRA], _packet[3] & 0x20, Input::BTN_EXTRA);
			}

			/* start new packet */
			_packet_idx = 0;
		}

		bool event_pending() { return _aux.data_read_ready(); }
};

#endif /* _DRIVERS__INPUT__SPEC__PS2__PS2_MOUSE_H_ */
