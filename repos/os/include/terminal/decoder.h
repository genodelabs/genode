/*
 * \brief  Escape-sequence decoder
 * \author Norman Feske
 * \date   2011-06-06
 */

/*
 * Copyright (C) 2011-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _TERMINAL__DECODER_H_
#define _TERMINAL__DECODER_H_

#include <terminal/character_screen.h>

namespace Terminal { class Decoder; }


class Terminal::Decoder
{
	private:

		/**
		 * Return digit value of character
		 */
		static inline int digit(char c)
		{
			if (c >= '0' && c <= '9') return c - '0';
			return -1;
		}

		/**
		 * Return true if character is a digit
		 */
		static inline bool is_digit(char c)
		{
			return (digit(c) >= 0);
		}

		/**
		 * Return true if number starts with the specified digit
		 *
		 * \param digit  digit 0..9 to test for
		 */
		static inline bool starts_with_digit(int digit, int number)
		{
			for (; number > 9; number /= 10);
			return (number == digit);
		}

		/**
		 * Return number with the first digit removed
		 */
		static inline int remove_first_digit(int number)
		{
			int factor = 1;
			for (; number/factor > 9; factor *= 10);
			return number % factor;
		}

		/**
		 * Buffer used for collecting escape sequences
		 */
		class Escape_stack
		{
			public:

				struct Entry
				{
					enum { INVALID, NUMBER, CODE };

					int type  = INVALID;
					int value = 0;
				};

				struct Number_entry : Entry
				{
					Number_entry(int number) { type = NUMBER, value = number; }
				};

				struct Code_entry : Entry
				{
					Code_entry(int code) { type = CODE, value = code; }
				};

				struct Invalid_entry : Entry { };

			private:

				enum { MAX_ENTRIES = 16 };
				Entry _entries[MAX_ENTRIES];
				int   _index;

				void _dump() const
				{
					Genode::log("--- escape stack follows ---");
					for (int i = 0; i < _index; i++) {
						int type = _entries[i].type;
						int value = _entries[i].value;
						Genode::log(type == Entry::INVALID ? " INVALID" :
						            type == Entry::NUMBER  ? " NUMBER "
						                                   : " CODE   ",
						            " ", value, " ",
						            "(", Genode::Hex(value), ")");
					}
				}

			public:

				Escape_stack() : _index(0) { }

				void reset() { _index = 0; }

				void push(Entry const &entry)
				{
					if (_index == MAX_ENTRIES - 1) {
						Genode::error("escape stack overflow");
						_dump();
						reset();
						return;
					}
					_entries[_index++] = entry;
				}

				/**
				 * Return number of stack elements
				 */
				unsigned num_elem() const { return _index; }

				/**
				 * Return Nth stack entry
				 *
				 * 'index' is relative to the bottom of the stack.
				 */
				Entry operator [] (int index)
				{
					return (index <= _index) ? _entries[index] : Invalid_entry();
				}

		} _escape_stack;

		enum State {
			STATE_IDLE,
			STATE_ESC_SEQ,    /* read escape sequence */
			STATE_ESC_NUMBER  /* read number argument within escape sequence */
		} _state;

		Character_screen &_screen;

		int _number; /* current number argument supplied in escape sequence */

		void _append_to_number(char c)
		{
			_number = _number*10 + digit(c);
		}

		void _enter_state_idle()
		{
			_state = STATE_IDLE;
			_escape_stack.reset();
		}

		void _enter_state_esc_seq()
		{
			_state = STATE_ESC_SEQ;
			_escape_stack.reset();
		}

		void _enter_state_esc_number()
		{
			_state = STATE_ESC_NUMBER;
			_number = 0;
		}

		/**
		 * Try to handle single-element escape sequence
		 *
		 * \return true if escape sequence was handled
		 */
		bool _handle_esc_seq_1()
		{
			switch (_escape_stack[0].value) {
			case '7': return (_screen.sc(),  true);
			case 'c': return (_screen.ris(), true);
			case 'H': return (_screen.hts(), true);
			case 'M': return (_screen.ri(),  true);
			default:  return false;
			}
		}

		bool _handle_esc_seq_2()
		{
			switch (_escape_stack[0].value) {

			case '[':

				switch (_escape_stack[1].value) {
				case 'A': return (_screen.cuu1(), true);
				case 'C': return (_screen.cuf(1), true);
				case 'c': return (_screen.u9(),   true);
				case 'H': return (_screen.home(), true);
				case 'J': return (_screen.ed(),   true);
				case 'K': return (_screen.el(),   true);
				case 'L': return (_screen.il(1),  true);
				case 'M': return (_screen.dl(1),  true);
				case 'P': return (_screen.dch(1), true);
				case '@': return (_screen.ich(1), true);
				case 'R': return (_screen.cpr(),  true);
				default:  return false;
				}
				break;

			case ']':

				switch (_escape_stack[1].value) {
				case 'R': return (_screen.oc(), true);
				default : return false;
				}

			case 8:

				return (_escape_stack[1].value == 'A') && (_screen.rc(), true);

			default: return false;
			}
			return false;
		}

		bool _handle_esc_seq_3()
		{

			/*
			 * All three-element sequences have the form \E[<NUMBER><COMMAND>
			 */
			if ((_escape_stack[0].value != '[')
			 || (_escape_stack[1].type  != Escape_stack::Entry::NUMBER))
				return false;

			int  const p1      = _escape_stack[1].value;
			char const command = _escape_stack[2].value;

			switch (command) {
			case 'm':
				if (p1 < 30)
					return (_screen.sgr(p1), true);

				/* p1 starting with digit '3' -> set foreground color */
				if (starts_with_digit(3, p1))
					return (_screen.setaf(remove_first_digit(p1)), true);

				/* p1 starting with digit '4' -> set background color */
				if (starts_with_digit(4, p1))
					return (_screen.setab(remove_first_digit(p1)), true);

			case 'D': return (_screen.cub(p1), true);
			case 'd': return (_screen.vpa(p1), true);
			case 'g': return (p1 == 3) && (_screen.tbc(), true);
			case 'G': return (_screen.hpa(p1), true);
			case 'h': return (p1 == 4) && (_screen.smir(), true);
			case 'K': return ((p1 == 0) && (_screen.el(),  true))
			              || ((p1 == 1) && (_screen.el1(), true));
			case 'l': return (p1 == 4) && (_screen.rmir(), true);
			case 'L': return (_screen.il(p1), true);
			case 'M': return (_screen.dl(p1), true);
			case 'n': return (p1 == 6) && (_screen.u7(), true);
			case 'P': return (_screen.dch(p1), true);
			case '@': return (_screen.ich(p1), true);
			case 'X': return (_screen.ech(p1), true);
			case 'C': return (_screen.cuf(p1), true);

			default: return false;
			}
		}

		bool _handle_esc_seq_4()
		{
			/*
			 * All four-element escape sequences have the form
			 * \E[?<NUMBER><COMMAND>
			 */
			if ((_escape_stack[0].value != '[')
			 || (_escape_stack[1].value != '?')
			 || (_escape_stack[2].type  != Escape_stack::Entry::NUMBER))
			 	return false;

			int  const p1      = _escape_stack[2].value;
			char const command = _escape_stack[3].value;

			switch (command) {
			case 'l':
				if (p1 ==  7) return (_screen.rmam(),  true);
				if (p1 == 25) return (_screen.civis(), true);
				return false;
			case 'h':
				if (p1 ==  7) return (_screen.smam(),  true);
				if (p1 == 25) return (_screen.cnorm(), true);
				return false;
			case 'c':
				if (p1 == 0) return true; /* appended to cnorm */
				if (p1 == 1) return true; /* appended to civis */
				if (p1 == 6) return (_screen.u8(),    true);
				if (p1 == 8) return (_screen.cvvis(), true);
				return false;
			default: return false;
			}
		}

		bool _handle_esc_seq_5()
		{
			/*
			 * All five-element escape sequences have the form
			 * \E[<NUMBER1>;<NUMBER2><COMMAND>
			 */
			if ((_escape_stack[0].value != '[')
			 || (_escape_stack[1].type  != Escape_stack::Entry::NUMBER)
			 || (_escape_stack[2].value != ';')
			 || (_escape_stack[3].type  != Escape_stack::Entry::NUMBER))
				return false;

			int const p[2]    = { _escape_stack[1].value,
			                      _escape_stack[3].value };
			int const command = _escape_stack[4].value;

			switch (command) {
			case 'r': return (_screen.csr(p[0], p[1]), true);
			case 'H': return (_screen.cup(p[0], p[1]), true);
			case 'm': {
				bool result = false;

				for (int i = 0; i < 2; i++) {

					if (p[i] == 0) {
						/* turn off all attributes */
						_screen.sgr0();
						result = true;

					} else if (p[i] == 1) {
						 /*
						  * attribute
						  *   1 bold (turn into highlight)
						  */
						_screen.sgr(p[i]);
						result = true;

					} else if ((p[i] >= 30) && (p[i] <= 37)) {
						/*
						 * color
						 *   30...37 text colors
						 *   40...47 background colors
						 */
						_screen.setaf(p[i] - 30);
						return true;

					} else if ((p[i] == 39) && (p[!i] == 49))
						return (_screen.op(),   true);

				}
				return result;
			}
			case 'R': return (_screen.u6(p[0], p[1]), true);
			default: return false;
			}
		}

		bool _handle_esc_seq_7()
		{
			/*
			 * All six-element escape sequences have the form
			 * \E[<NUMBER1>;<NUMBER2>;<NUMBER3><COMMAND>
			 */

			if ((_escape_stack[0].value != '[')
			 || (_escape_stack[1].type  != Escape_stack::Entry::NUMBER)
			 || (_escape_stack[2].value != ';')
			 || (_escape_stack[3].type  != Escape_stack::Entry::NUMBER)
			 || (_escape_stack[4].value != ';')
			 || (_escape_stack[5].type  != Escape_stack::Entry::NUMBER))
				return false;

			int const p1      = _escape_stack[1].value;
			int const p2      = _escape_stack[2].value;
			int const p3      = _escape_stack[3].value;
			int const command = _escape_stack[6].value;

			switch (command) {
			case 'm':

				/*
				 * Currently returning true w/o actually handling the
				 * sequence
				 */
				Genode::warning("Sequence '[", p1, ";", p2, ";", p3, "m' is not implemented");
				return true;
			default: return false;
			}

			return true;
		}

	public:

		Decoder(Character_screen &screen)
		: _state(STATE_IDLE), _screen(screen), _number(0) { }

		void insert(unsigned char c)
		{
			switch (_state) {

			case STATE_IDLE:

				enum { ESC_PREFIX = 0x1b };
				if (c == ESC_PREFIX) {
					_enter_state_esc_seq();
					break;
				}

				/* handle special characters */

				/* handle normal characters */
				_screen.output(c);

				break;

			case STATE_ESC_SEQ:

				/*
				 * We received the prefix character of an escape sequence,
				 * collect the escape-sequence elements until we detect the
				 * completion of the sequence.
				 */

				/* check for start of a number argument */
				if (is_digit(c) && !_number)
				{
					_enter_state_esc_number();
					_append_to_number(c);
					break;
				}

				/* non-number character of escape sequence */
				_escape_stack.push(Escape_stack::Code_entry(c));
				break;

			case STATE_ESC_NUMBER:

				/*
				 * We got the first character belonging to a number
				 * argument of an escape sequence. Keep reading digits.
				 */
				if (is_digit(c)) {
					_append_to_number(c);
					break;
				}

				/*
				 * End of number is reached.
				 */

				/* push the complete number to the escape stack */
				_escape_stack.push(Escape_stack::Number_entry(_number));
				_number = 0;

				/* push non-number character as commend entry */
				_escape_stack.push(Escape_stack::Code_entry(c));

				break;
			}

			/*
			 * Check for the completeness of an escape sequence.
			 */
			if (((_escape_stack.num_elem() == 1) && _handle_esc_seq_1())
			 || ((_escape_stack.num_elem() == 2) && _handle_esc_seq_2())
			 || ((_escape_stack.num_elem() == 3) && _handle_esc_seq_3())
			 || ((_escape_stack.num_elem() == 4) && _handle_esc_seq_4())
			 || ((_escape_stack.num_elem() == 5) && _handle_esc_seq_5())
			 || ((_escape_stack.num_elem() == 7) && _handle_esc_seq_7()))
				_enter_state_idle();
		};
};

#endif /* _TERMINAL__DECODER_H_ */
