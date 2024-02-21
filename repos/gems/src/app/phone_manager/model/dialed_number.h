/*
 * \brief  Dialed number
 * \author Norman Feske
 * \date   2018-05-23
 */

/*
 * Copyright (C) 2018 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _MODEL__DIALED_NUMBER_H_
#define _MODEL__DIALED_NUMBER_H_

#include <base/output.h>
#include <util/utf8.h>
#include <types.h>
#include <xml.h>

namespace Sculpt { struct Dialed_number; }


struct Sculpt::Dialed_number : Noncopyable
{
	public:

		struct Digit
		{
			char value;

			bool valid() const { return (value >= '0' && value <= '9')
			                         || (value == '#')
			                         || (value == '*'); }

			void print(Output &out) const
			{
				if (valid())
					Genode::print(out, Char(value));
			}
		};

	private:

		enum { CAPACITY = 32 };
		Digit _digits[CAPACITY] { };

		unsigned _length = 0;

	public:

		void print(Output &out) const
		{
			for (unsigned i = 0; i < _length; i++)
				Genode::print(out, _digits[i]);
		}

		void append_digit(Digit d)
		{
			/* ignore out-of-range digit values */
			if (!d.valid())
				return;

			if (_length < CAPACITY) {
				_digits[_length] = d;
				_length++;
			}
		}

		void remove_last_digit()
		{
			if (_length > 0) {
				_length--;
				_digits[_length].value = 0;
			}
		}

		bool suitable_for_call() const { return _length >= 3; }

		bool at_least_one_digit() const { return _length > 0; }
};

#endif /* _MODEL__DIALED_NUMBER_H_ */
