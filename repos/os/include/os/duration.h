/*
 * \brief  A duration type for both highly precise and long durations
 * \author Martin Stein
 * \date   2017-03-21
 */

/*
 * Copyright (C) 2016-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _OS__DURATION_H_
#define _OS__DURATION_H_

/* Genode includes */
#include <base/stdint.h>
#include <base/exception.h>

namespace Genode {

	class Microseconds;
	class Milliseconds;
	class Duration;
}


/**
 * Makes it clear that a given integer value stands for microseconds
 */
struct Genode::Microseconds
{
	unsigned long value;

	explicit Microseconds(unsigned long value) : value(value) { }
};


/**
 * Makes it clear that a given integer value stands for milliseconds
 */
struct Genode::Milliseconds
{
	unsigned long value;

	explicit Milliseconds(unsigned long value) : value(value) { }
};


/**
 * A duration type that combines high precision and large intervals
 */
struct Genode::Duration
{
	public:

		struct Overflow : Exception { };

	private:

		enum { US_PER_MS    = 1000UL                  };
		enum { MS_PER_HOUR  = 1000UL * 60 * 60        };
		enum { US_PER_HOUR  = 1000UL * 1000 * 60 * 60 };

		unsigned long _microseconds { 0 };
		unsigned long _hours        { 0 };

		void _add_us_less_than_an_hour(unsigned long us);
		void _raise_hours(unsigned long hours);

	public:

		void add(Microseconds us);
		void add(Milliseconds ms);

		bool less_than(Duration &other) const;

		explicit Duration(Milliseconds ms) { add(ms); }
		explicit Duration(Microseconds us) { add(us); }

		Microseconds trunc_to_plain_us() const;
};

#endif /* _OS__DURATION_H_ */
