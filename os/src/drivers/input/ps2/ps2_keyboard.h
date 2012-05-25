/*
 * \brief  PS/2 keyboard protocol handler
 * \author Norman Feske
 * \date   2007-10-08
 */

/*
 * Copyright (C) 2007-2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _PS2_KEYBOARD_H_
#define _PS2_KEYBOARD_H_

#include <base/printf.h>
#include <input/event_queue.h>
#include <input/keycodes.h>

#include "input_driver.h"
#include "serial_interface.h"
#include "scan_code_set_1.h"
#include "scan_code_set_2.h"

class Ps2_keyboard : public Input_driver
{
	private:

		static const bool verbose            = false;
		static const bool verbose_scan_codes = false;

		Serial_interface &_kbd;
		Event_queue      &_ev_queue;
		bool              _xlate_mode;

		/**
		 * Array for tracking the current keyboard state
		 */
		bool _key_state[Input::KEY_MAX + 1];


		/**
		 * Interface for keyboard-packet state machine
		 */
		class Scan_code_state_machine
		{
			public:

				/**
				 * Constructor
				 */
				Scan_code_state_machine() { reset(); }

				virtual ~Scan_code_state_machine() { }

				/**
				 * Reset state machine
				 */
				virtual void reset() { }

				/**
				 * Process value received from the keyboard
				 */
				virtual void process(unsigned char v) = 0;

				/**
				 * Return true if packet is complete
				 */
				virtual bool ready() = 0;

				/**
				 * Return true if event is a press event
				 */
				virtual bool press() = 0;

				/**
				 * Return key code of current packet
				 */
				virtual unsigned int key_code() = 0;
		};


		/**
		 * State machine for parsing scan-code-set 1 packets
		 */
		class Scan_code_set_1_state_machine : public Scan_code_state_machine
		{
			enum State {
				READ_FIRST, READ_E0_VALUE, READ_E1_VALUE,
				PAUSE_READ_ADDITIONAL_VALUE
			};

			enum Type { NORMAL, EXT_E0, EXT_E1, PAUSE } _type;

			State    _state;     /* current state of packet processing */
			bool     _press;     /* true if key-press event            */
			bool     _ready;     /* packet complete                    */
			unsigned _key_code;  /* key code of complete packet        */

			public:

				/**
				 * Constructor
				 */
				Scan_code_set_1_state_machine()
				{
					/* init scan-code table */
					init_scan_code_set_1_0xe0();
				}


				/******************************************
				 ** Interface of Scan-code state machine **
				 ******************************************/

				void reset()
				{
					_state    = READ_FIRST;
					_ready    = false;
					_press    = false;
					_key_code = 0;
				}

				void process(unsigned char v)
				{
					if (verbose_scan_codes)
						PLOG("process %02x", v);

					switch (_state) {

					case READ_FIRST:

						if (v == 0xe0) {
							_state = READ_E0_VALUE;
							return;
						}
						if (v == 0xe1) {
							_state = READ_E1_VALUE;
							return;
						}

						/* normal packet (one byte) is complete */
						_type = NORMAL;
						break;

					case READ_E0_VALUE:

						/* drop fake shifts */
						if ((v & 0x7f) == 0x2a) {
							reset();
							return;
						}

						/* e0 packet is complete */
						_type  = EXT_E0;
						break;

					case READ_E1_VALUE:

						/*
						 * Pause is a sequence of 6 bytes. The first three
						 * bytes represent the press event and the second three
						 * bytes represent an artificial release event that
						 * immediately follows the press event (the real
						 * release event cannot be detected). Both sub
						 * sequences start with 0xe1 such that we can handle
						 * each sub sequence as an e1 packet except that we
						 * have to read an additional argument (0x2a or 0x37
						 * respectively).
						 */
						if (v == 0x1d || v == 0x9d) {
							_state = PAUSE_READ_ADDITIONAL_VALUE;
							return;
						}

						/* no pause, e1 packet is complete */
						_type  = EXT_E1;
						break;

					case PAUSE_READ_ADDITIONAL_VALUE:

						/* pause sub sequence complete */
						_type = PAUSE;
						break;
					}

					/* the most significant bit signals release event */
					_press = !(v & 0x80);

					/* keep the remaining bits for scan-code translation */
					v &= ~0x80;

					/* convert scan code to unified key code */
					switch (_type) {
					case NORMAL: _key_code = scan_code_set_1[v];      break;
					case EXT_E0: _key_code = scan_code_set_1_0xe0[v]; break;
					case EXT_E1: _key_code = Input::KEY_UNKNOWN;      break;
					case PAUSE:  _key_code = Input::KEY_PAUSE;        break;
					}

					/* packet is ready */
					_ready = true;
				}

				bool ready() { return _ready; }

				bool press() { return _press; }

				unsigned int key_code() { return ready() ? _key_code : Input::KEY_UNKNOWN; }

		} _scan_code_set_1_state_machine;


		/**
		 * State machine for parsing scan-code-set 2 packets
		 */
		class Scan_code_set_2_state_machine : public Scan_code_state_machine
		{
			enum State {
				READ_FIRST, READ_EXT, READ_RELEASE_VALUE,
				READ_PAUSE, READ_RELEASE_PAUSE,
			};

			State    _state;     /* current state of packet processing */
			bool     _press;     /* true if key-press event            */
			bool     _extended;  /* true if extended packet            */
			bool     _pause;     /* true if pause key packet           */
			bool     _ready;     /* packet complete                    */
			unsigned _key_code;  /* key code of complete packet        */

			public:

				/**
				 * Constructor
				 */
				Scan_code_set_2_state_machine()
				{
					/* init sparsely populated extended scan-code table */
					init_scan_code_set_2_ext();
				}


				/******************************************
				 ** Interface of Scan-code state machine **
				 ******************************************/

				void reset()
				{
					_state    = READ_FIRST;
					_press    = true;
					_extended = false;
					_pause    = false;
					_ready    = false;
					_key_code = 0;
				}

				void process(unsigned char v)
				{
					if (verbose_scan_codes)
						PLOG("process %02x", v);

					enum {
						EXTENDED_KEY_PREFIX = 0xe0,
						RELEASE_PREFIX      = 0xf0,
					};

					switch (_state) {

					case READ_FIRST:

						if (v == EXTENDED_KEY_PREFIX) {
							_state    = READ_EXT;
							_extended = true;
							return;
						}
						if (v == RELEASE_PREFIX) {
							_state = READ_RELEASE_VALUE;
							_press = false;
							return;
						}
						/*
						 * Pause key produces e1 14 77 resp. e1 f0 14 f0 77 and is
						 * folded into extended table.
						 */
						if (v == 0xe1) {
							_state    = READ_PAUSE;
							_extended = true;
							return;
						}
						break;

					case READ_EXT:

						/* drop fake shifts */
						if (v == 0x12) {
							reset();
							return;
						}

						if (v == RELEASE_PREFIX) {
							_state = READ_RELEASE_VALUE;
							_press = false;
							return;
						}
						break;

					case READ_RELEASE_VALUE:

						break;

					case READ_PAUSE:

						if (v == RELEASE_PREFIX) {
							_state = READ_RELEASE_PAUSE;
							_press = false;
							return;
						}
						/* eat 14 but stay in READ_PAUSE */
						if (v == 0x14)
							return;
						break;

					case READ_RELEASE_PAUSE:

						/* eat 14 and go to READ_PAUSE */
						if (v == 0x14) {
							_state = READ_PAUSE;
							return;
						}
						break;
					}

					/*
					 * Packet is complete, translate hardware scan code to
					 * unified key code.
					 */
					_ready    = true;
					_key_code = (_extended ? scan_code_set_2_ext : scan_code_set_2)[v];
				}

				bool ready() { return _ready; }

				bool press() { return _press; }

				unsigned int key_code() {
					return ready() ? _key_code : Input::KEY_UNKNOWN; }

		} _scan_code_set_2_state_machine;

		/**
		 * Used keyboard-packet state machine
		 */
		Scan_code_state_machine *_state_machine;

	public:

		/**
		 * Constructor
		 *
		 * \param xlate_mode  serial interface does only support scan-code set 1
		 *
		 * If 'xlate_mode' is true, we do not attempt to manually switch the
		 * keyboard to scan code set 2 but just decode the scan-code set 1.
		 */
		Ps2_keyboard(Serial_interface &kbd, Event_queue &ev_queue, bool xlate_mode)
		:
			_kbd(kbd), _ev_queue(ev_queue), _xlate_mode(xlate_mode)
		{
			for (int i = 0; i <= Input::KEY_MAX; i++)
				_key_state[i] = false;

			/* select state machine to use for packet processing */
			reset();

			/* prepare state machine for processing the first packet */
			_state_machine->reset();

			Genode::printf("Using keyboard with scan code set %s.\n",
			               _state_machine == &_scan_code_set_1_state_machine
			               ? _xlate_mode ? "1 (xlate)" : "1"
			               : "2");
		}

		void reset()
		{
			/* acknowledge code from keyboard */
			enum { ACK = 0xfa };

			/* scan-code request/config commands */
			enum { SCAN_CODE_REQUEST = 0, SCAN_CODE_SET_1 = 1, SCAN_CODE_SET_2 = 2 };

			_state_machine = &_scan_code_set_1_state_machine;
			if (_xlate_mode)
				return;

			/* try to enable scan-code set 2 */
			_kbd.write(0xf0);
			if (_kbd.read() != ACK) {
				PWRN("Scan code setting not supported");
				return;
			}
			_kbd.write(SCAN_CODE_SET_2);
			if (_kbd.read() != ACK) {
				PWRN("Scan code 2 not supported");
				return;
			}

			/*
			 * If configuration of scan-code set 2 was successful, select
			 * the corresponding state machine for decoding the packets.
			 */
			_state_machine = &_scan_code_set_2_state_machine;
		}


		/****************************
		 ** Input driver interface **
		 ****************************/

		void handle_event()
		{
			_state_machine->process(_kbd.read());

			if (!_state_machine->ready())
				return;

			bool     press    = _state_machine->press();
			unsigned key_code = _state_machine->key_code();

			/*
			 * The old key state should not equal the state after the event.
			 * Key-repeat events trigger this condition and are discarded.
			 */
			if (_key_state[key_code] == press) {
				_state_machine->reset();
				return;
			}

			/* remember new key state */
			_key_state[key_code] = _state_machine->press();

			if (verbose)
				PLOG("post %s, key_code = %d\n",
				     press ? "PRESS" : "RELEASE", key_code);

			/* post event to event queue */
			_ev_queue.add(Input::Event(press ? Input::Event::PRESS
			                                 : Input::Event::RELEASE,
			                           key_code, 0, 0, 0, 0));

			/* start with new packet */
			_state_machine->reset();
		}

		bool event_pending() { return _kbd.data_read_ready(); }
};

#endif /* _PS2_KEYBOARD_H_ */
