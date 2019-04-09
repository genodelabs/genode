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

#ifndef _INCLUDE__BASE__DURATION_H_
#define _INCLUDE__BASE__DURATION_H_

/* Genode includes */
#include <base/stdint.h>
#include <base/exception.h>
#include <base/output.h>

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
	uint64_t value;

	explicit Microseconds(uint64_t value) : value(value) { }

	void print(Output &out) const
	{
		Genode::print(out, value);
		out.out_string(" us");
	}
};


/**
 * Makes it clear that a given integer value stands for milliseconds
 */
struct Genode::Milliseconds
{
	uint64_t value;

	explicit Milliseconds(uint64_t value) : value(value) { }

	void print(Output &out) const
	{
		Genode::print(out, value);
		out.out_string(" ms");
	}
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

		uint64_t _microseconds { 0 };

	public:

		void add(Microseconds us);
		void add(Milliseconds ms);

		bool less_than(Duration const &other) const;

		explicit Duration(Milliseconds ms) { add(ms); }
		explicit Duration(Microseconds us) { add(us); }

		Microseconds trunc_to_plain_us() const;
		Milliseconds trunc_to_plain_ms() const;
};

namespace Genode
{
	static inline
	Microseconds min(Microseconds const x, Microseconds const y) {
		return (x.value < y.value) ? x : y; }

	static inline
	Microseconds max(Microseconds const x, Microseconds const y) {
		return (x.value > y.value) ? x : y; }

	static inline
	Milliseconds min(Milliseconds const x, Milliseconds const y) {
		return (x.value < y.value) ? x : y; }

	static inline
	Milliseconds max(Milliseconds const x, Milliseconds const y) {
		return (x.value > y.value) ? x : y; }
};

#endif /* _INCLUDE__BASE__DURATION_H_ */
