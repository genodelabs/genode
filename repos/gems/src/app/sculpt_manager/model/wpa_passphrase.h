/*
 * \brief  WPA passphrase
 * \author Norman Feske
 * \date   2018-05-23
 */

/*
 * Copyright (C) 2018 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _MODEL__WPA_PASSPHRASE_H_
#define _MODEL__WPA_PASSPHRASE_H_

#include <base/output.h>
#include <util/utf8.h>
#include <types.h>
#include <xml.h>

namespace Sculpt {
	struct Blind_wpa_passphrase;
	struct Wpa_passphrase;
}


struct Sculpt::Blind_wpa_passphrase
{
	virtual void print_bullets(Output &) const = 0;

	virtual ~Blind_wpa_passphrase() { }

	void print(Output &out) const { print_bullets(out); }

	virtual bool suitable_for_connect() const = 0;
};


struct Sculpt::Wpa_passphrase : Blind_wpa_passphrase
{
	private:

		enum { CAPACITY = 64 };
		Codepoint _characters[CAPACITY] { };

		unsigned _length = 0;

	public:

		/**
		 * Print PSK as UTF-8 string
		 */
		void print(Output &out) const
		{
			/*
			 * XXX duplicated from gems/src/server/terminal/main.cc
			 */
			struct Utf8 { char b0, b1, b2, b3, b4; };

			auto utf8_from_codepoint = [] (unsigned c) {

				/* extract 'n' bits 'at' bit position of value 'c' */
				auto bits = [c] (unsigned at, unsigned n) {
					return (c >> at) & ((1 << n) - 1); };

				return (c < 2<<7)  ? Utf8 { char(bits( 0, 7)), 0, 0, 0, 0 }
				     : (c < 2<<11) ? Utf8 { char(bits( 6, 5) | 0xc0),
				                            char(bits( 0, 6) | 0x80), 0, 0, 0 }
				     : (c < 2<<16) ? Utf8 { char(bits(12, 4) | 0xe0),
				                            char(bits( 6, 6) | 0x80),
				                            char(bits( 0, 6) | 0x80), 0, 0 }
				     : (c < 2<<21) ? Utf8 { char(bits(18, 3) | 0xf0),
				                            char(bits(12, 6) | 0x80),
				                            char(bits( 6, 6) | 0x80),
				                            char(bits( 0, 6) | 0x80), 0 }
				     : Utf8 { };
			};

			for (unsigned i = 0; i < _length; i++) {

				Utf8 const utf8 = utf8_from_codepoint(_characters[i].value);

				auto _print = [&] (char c) {
					if (c)
						Genode::print(out, Char(c)); };

				_print(utf8.b0); _print(utf8.b1); _print(utf8.b2);
				_print(utf8.b3); _print(utf8.b4);
			}
		}

		void append_character(Codepoint c)
		{
			if (_length < CAPACITY) {
				_characters[_length] = c;
				_length++;
			}
		}

		void remove_last_character()
		{
			if (_length > 0) {
				_length--;
				_characters[_length].value = 0;
			}
		}

		/**
		 * Print passphrase as a number of bullets
		 */
		void print_bullets(Output &out) const override
		{
			char const bullet_utf8[4] = { (char)0xe2, (char)0x80, (char)0xa2, 0 };
			for (unsigned i = 0; i < _length; i++)
				Genode::print(out, bullet_utf8);
		}

		bool suitable_for_connect() const override { return _length >= 8; }
};

#endif /* _MODEL__WPA_PASSPHRASE_H_ */
