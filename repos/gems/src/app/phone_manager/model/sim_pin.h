/*
 * \brief  SIM pin
 * \author Norman Feske
 * \date   2018-05-23
 */

/*
 * Copyright (C) 2018 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _MODEL__SIM_PIN_H_
#define _MODEL__SIM_PIN_H_

#include <base/output.h>
#include <util/utf8.h>
#include <types.h>
#include <xml.h>


namespace Sculpt { struct Bullet; }

struct Sculpt::Bullet
{
	void print(Output &out) const
	{
		char const bullet_utf8[4] = { (char)0xe2, (char)0x80, (char)0xa2, 0 };
		Genode::print(out, bullet_utf8);
	}
};


namespace Sculpt { struct Blind_sim_pin; }


struct Sculpt::Blind_sim_pin
{
	virtual void print_bullets(Output &) const = 0;

	virtual ~Blind_sim_pin() { }

	void print(Output &out) const { print_bullets(out); }

	virtual bool suitable_for_unlock() const = 0;

	virtual bool at_least_one_digit() const = 0;
};


namespace Sculpt { struct Sim_pin; }


struct Sculpt::Sim_pin : Blind_sim_pin
{
	public:

		struct Digit { unsigned value; };

	private:

		enum { CAPACITY = 4 };
		Digit _digits[CAPACITY] { };

		unsigned _length = 0;

	public:

		bool confirmed = false;

		void print(Output &out) const
		{
			for (unsigned i = 0; i < _length; i++)
				Genode::print(out, _digits[i].value);
		}

		void append_digit(Digit d)
		{
			/* ignore out-of-range digit values */
			if (d.value > 9)
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

		void print_bullets(Output &out) const override
		{
			for (unsigned i = 0; i < _length; i++)
				Genode::print(out, Bullet());
		}

		bool suitable_for_unlock() const override { return _length == 4; }

		bool at_least_one_digit() const override { return _length > 0; }

		bool operator != (Sim_pin const &other) const
		{
			auto any_digit_differs = [&] (auto a, auto b)
			{
				for (unsigned i = 0; i < _length; i++)
					if (a[i].value != b[i].value)
						return true;
				return false;
			};

			return (_length != other._length)
			    || any_digit_differs(_digits, other._digits);
		}
};

#endif /* _MODEL__SIM_PIN_H_ */
