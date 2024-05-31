/*
 * \brief  Conversion of Genode input events to PS/2 scan codes
 * \author Norman Feske
 * \author Alexander Boettcher
 * \date   2013-08-20
 */

/*
 * Copyright (C) 2013-2017 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

#ifndef _SCAN_CODE_H_
#define _SCAN_CODE_H_

/* Genode includes */
#include <scan_code_set_1.h> /* included from os/src/driver/ps2 */


class Scan_code
{
	private:

		class Converter
		{
			public:

				unsigned char scan_code     [Input::KEY_UNKNOWN];
				unsigned char scan_code_ext [Input::KEY_UNKNOWN];

			private:

				unsigned char _search_scan_code(Input::Keycode keycode)
				{
					for (unsigned i = 0; i < SCAN_CODE_SET_1_NUM_KEYS; i++)
						if (scan_code_set_1[i] == keycode)
							return i;
					return 0;
				}

				unsigned char _search_scan_code_ext(Input::Keycode keycode)
				{
					for (unsigned i = 0; i < SCAN_CODE_SET_1_NUM_KEYS; i++)
						if (scan_code_set_1_0xe0[i] == keycode)
							return i;
					return 0;
				}

			public:

				Converter()
				{
					init_scan_code_set_1_0xe0();

					for (unsigned i = 0; i < Input::KEY_UNKNOWN; i++) {
						scan_code     [i] = _search_scan_code    ((Input::Keycode)i);
						scan_code_ext [i] = _search_scan_code_ext((Input::Keycode)i);
					}
				}
		};

		static Converter &converter()
		{
			static Converter inst;
			return inst;
		}

		Input::Keycode _keycode;

	public:

		Scan_code(Input::Keycode keycode) : _keycode(keycode) { }

		bool normal() const { return converter().scan_code[_keycode]; }

		bool valid() const
		{
			return normal() || ext();
		}

		unsigned char code() const
		{
			return converter().scan_code[_keycode];
		}

		unsigned char ext() const
		{
			return converter().scan_code_ext[_keycode];
		}
};

#endif /* _SCAN_CODE_H_ */
