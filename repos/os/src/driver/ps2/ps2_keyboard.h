/*
 * \brief  PS/2 keyboard protocol handler
 * \author Norman Feske
 * \date   2007-10-08
 */

/*
 * Copyright (C) 2007-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _DRIVERS__INPUT__SPEC__PS2__PS2_KEYBOARD_H_
#define _DRIVERS__INPUT__SPEC__PS2__PS2_KEYBOARD_H_

#include <base/log.h>
#include <input/event_queue.h>
#include <input/keycodes.h>

#include "input_driver.h"
#include "serial_interface.h"
#include "scan_code_set_1.h"
#include "scan_code_set_2.h"
#include "verbose.h"

namespace Ps2 { class Keyboard; }

class Ps2::Keyboard : public Input_driver
{
	private:

		/*
		 * Noncopyable
		 */
		Keyboard(Keyboard const &);
		Keyboard &operator = (Keyboard const &);

		Serial_interface   &_kbd;
		bool         const  _xlate_mode;
		Verbose      const &_verbose;

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
				virtual void process(unsigned char v, bool) = 0;

				/**
				 * Return true if packet is complete
				 */
				virtual bool ready() const = 0;

				/**
				 * Return true if event is a press event
				 */
				virtual bool press() const = 0;

				/**
				 * Return key code of current packet
				 */
				virtual unsigned int key_code() const = 0;
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

			enum Type { NORMAL, EXT_E0, EXT_E1, PAUSE } _type { NORMAL };

			State    _state    = READ_FIRST;  /* current state of packet processing */
			bool     _press    = false;       /* true if key-press event            */
			bool     _ready    = false;       /* packet complete                    */
			unsigned _key_code = 0;           /* key code of complete packet        */

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

				void reset() override
				{
					_state    = READ_FIRST;
					_ready    = false;
					_press    = false;
					_key_code = 0;
				}

				void process(unsigned char v, bool verbose) override
				{
					if (verbose)
						Genode::log("process ", Genode::Hex(v), " scan code set 1");

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
						if ((v & 0x7f) == 0x2a || (v & 0x7f) == 0x36) {
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
					v &= (unsigned char)(~0x80);

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

				bool ready() const override { return _ready; }
				bool press() const override { return _press; }

				unsigned int key_code() const override
				{
					return ready() ? _key_code : (unsigned)Input::KEY_UNKNOWN;
				}

		} _scan_code_set_1_state_machine { };


		/**
		 * State machine for parsing scan-code-set 2 packets
		 */
		class Scan_code_set_2_state_machine : public Scan_code_state_machine
		{
			enum State {
				READ_FIRST, READ_EXT, READ_RELEASE_VALUE,
				READ_PAUSE, READ_RELEASE_PAUSE,
			};

			State    _state    = READ_FIRST;  /* current state of packet processing */
			bool     _press    = false;       /* true if key-press event            */
			bool     _extended = false;       /* true if extended packet            */
			bool     _pause    = false;       /* true if pause key packet           */
			bool     _ready    = false;       /* packet complete                    */
			unsigned _key_code = 0;           /* key code of complete packet        */

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

				void reset() override
				{
					_state    = READ_FIRST;
					_press    = true;
					_extended = false;
					_pause    = false;
					_ready    = false;
					_key_code = 0;
				}

				void process(unsigned char v, bool verbose) override
				{
					if (verbose)
						Genode::log("process ", Genode::Hex(v), " scan code set 2");

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
						if (v == 0x12 || v == 0x59) {
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

				bool ready() const override { return _ready; }
				bool press() const override { return _press; }

				unsigned int key_code() const override
				{
					return ready() ? _key_code : (unsigned)Input::KEY_UNKNOWN;
				}

		} _scan_code_set_2_state_machine { };

		/* acknowledge code from keyboard */
		enum { ACK = 0xfa };

		/**
		 * Used keyboard-packet state machine
		 */
		Scan_code_state_machine *_state_machine = nullptr;

		struct Led_state
		{
			bool capslock = false;
			bool numlock  = false;
			bool scrlock  = false;

			bool operator == (Led_state const &other) const {
				return other.capslock == capslock
				    && other.numlock  == numlock
				    && other.scrlock  == scrlock; }
		};

		Led_state _led_state      { };
		Led_state _next_led_state { };

		void _set_leds(bool capslock, bool numlock, bool scrlock)
		{
			_kbd.apply_commands([&]() {
				_kbd.write(0xed);
				if (_kbd.read() != ACK) {
					Genode::warning("setting of mode indicators failed (0xed)");
					return;
				}

				_kbd.write((unsigned char)((capslock ? 4 : 0) |
				                           (numlock  ? 2 : 0) |
				                           (scrlock  ? 1 : 0)));
				if (_kbd.read() != ACK) {
					Genode::warning("setting of mode indicators failed");
					return;
				}
			});
		}

		void _update_leds()
		{
			/* don't interfere with pending events when applying next led state */
			if (event_pending() || _led_state == _next_led_state)
				return;

			_set_leds(_next_led_state.capslock,
			          _next_led_state.numlock,
			          _next_led_state.scrlock);

			_led_state = _next_led_state;
		}

	public:

		/**
		 * Constructor
		 *
		 * \param xlate_mode  serial interface does only support scan-code set 1
		 *
		 * If 'xlate_mode' is true, we do not attempt to manually switch the
		 * keyboard to scan code set 2 but just decode the scan-code set 1.
		 */
		Keyboard(Serial_interface &kbd, bool xlate_mode, Verbose const &verbose)
		:
			_kbd(kbd), _xlate_mode(xlate_mode), _verbose(verbose)
		{
			for (int i = 0; i <= Input::KEY_MAX; i++)
				_key_state[i] = false;

			/* select state machine to use for packet processing */
			reset();

			/* prepare state machine for processing the first packet */
			_state_machine->reset();

			Genode::log("Using keyboard with scan code set ",
			            _state_machine == &_scan_code_set_1_state_machine
			            ? _xlate_mode ? "1 (xlate)" : "1"
			            : "2");
		}

		enum Led { CAPSLOCK_LED, NUMLOCK_LED, SCRLOCK_LED };

		void led_enabled(Led led, bool enabled)
		{
			switch (led) {
			case CAPSLOCK_LED: _next_led_state.capslock = enabled; break;
			case NUMLOCK_LED:  _next_led_state.numlock  = enabled; break;
			case SCRLOCK_LED:  _next_led_state.scrlock  = enabled; break;
			}
			_update_leds();
		}

		void reset()
		{
			/*
			 * We enforce an initial LED state with all indicators switched
			 * off. This also informs notebook keyboards (which use normal keys
			 * as numeric pad if numlock is enabled) about our initial
			 * assumption.
			 */
			_set_leds(false, false, false);

			/* scan-code request/config commands */
			enum { SCAN_CODE_REQUEST = 0, SCAN_CODE_SET_1 = 1, SCAN_CODE_SET_2 = 2 };

			_state_machine = &_scan_code_set_1_state_machine;
			if (_xlate_mode)
				return;

			_kbd.apply_commands([&]() {
				/* try to enable scan-code set 2 */
				_kbd.write(0xf0);
				if (_kbd.read() != ACK) {
					Genode::warning("scan code setting not supported");
					return;
				}
				_kbd.write(SCAN_CODE_SET_2);
				if (_kbd.read() != ACK) {
					Genode::warning("scan code 2 not supported");
					return;
				}
			});

			/*
			 * If configuration of scan-code set 2 was successful, select
			 * the corresponding state machine for decoding the packets.
			 */
			_state_machine = &_scan_code_set_2_state_machine;
		}


		/****************************
		 ** Input driver interface **
		 ****************************/

		void handle_event(Event::Session_client::Batch &batch) override
		{
			_state_machine->process(_kbd.read(), _verbose.scancodes);

			if (!_state_machine->ready())
				return;

			bool           press    = _state_machine->press();
			Input::Keycode key_code = Input::Keycode(_state_machine->key_code());

			/*
			 * The old key state should not equal the state after the event.
			 * Key-repeat events trigger this condition and are discarded.
			 */
			if (_key_state[key_code] == press) {
				_state_machine->reset();
				_update_leds();
				return;
			}

			/* remember new key state */
			_key_state[key_code] = _state_machine->press();

			if (_verbose.keyboard)
				Genode::log("post ", press ? "PRESS" : "RELEASE", ", "
				            "key_code = ", Input::key_name(key_code));

			/* post event to event queue */
			if (press)
				batch.submit(Input::Press{key_code});
			else
				batch.submit(Input::Release{key_code});

			/* start with new packet */
			_state_machine->reset();
			_update_leds();
		}

		bool event_pending() const override { return _kbd.data_read_ready(); }
};

#endif /* _DRIVERS__INPUT__SPEC__PS2__PS2_KEYBOARD_H_ */
