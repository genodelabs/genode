/*
 * \brief  Utility to determine the median of N values
 * \author Norman Feske
 * \date   2023-12-13
 */

/*
 * Copyright (C) 2024 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _MEDIAN_H_
#define _MEDIAN_H_

namespace Mixer { template <typename T, unsigned N> struct Median; }


template <typename T, unsigned N>
struct Mixer::Median
{
	T sorted[N] { };

	unsigned n = 0;

	void capture(T v)
	{
		if (n >= N)
			return;

		unsigned pos = 0;

		while ((pos < n) && (v > sorted[pos]))
			pos++;

		n++;
		for (unsigned i = n; i > pos; i--)
			sorted[i] = sorted[i - 1];

		sorted[pos] = v;
	}

	T median() const { return sorted[n/2]; }

	T jitter() const
	{
		if (n < 2)
			return { };

		return max(median() - sorted[0], sorted[n - 1] - median());
	}

	void print(Output &out) const
	{
		for (unsigned i = 0; i < n; i++) {
			Genode::print(out, sorted[i]);
			if (i + 1 < n)
				Genode::print(out, ", ");
		}
	}
};

#endif /* _MEDIAN_H_ */

