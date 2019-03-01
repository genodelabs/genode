/*
 * \brief  Utilities for string handling
 * \author Norman Feske
 * \date   2019-03-01
 */

/*
 * Copyright (C) 2019 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _STRING_H_
#define _STRING_H_

/* Genode includes */
#include <util/string.h>
#include <base/output.h>

/* local includes */
#include "types.h"

namespace Sculpt {

	class Subst;
	class Pretty;
}


class Sculpt::Subst
{
	private:

		char const * const _pattern;
		size_t       const _pattern_len = strlen(_pattern);
		char const * const _replacement;
		char const * const _input;

	public:

		/*
		 * This utility is currently limited to strings. It should better
		 * act as a filter that accepts any printable type as 'input'
		 * argument. However the automatic deduction of class template
		 * arguments from a constuctor calls is not suppored before C++17.
		 */

		Subst(char const *pattern, char const *replacement, char const *input)
		: _pattern(pattern), _replacement(replacement), _input(input) { }

		template <size_t N>
		Subst(char const *pattern, char const *replacement, String<N> const &input)
		: Subst(pattern, replacement, input.string()) { }

		void print(Output &out) const
		{
			for (char const *curr = _input; *curr; ) {
				if (_pattern_len && strcmp(curr, _pattern, _pattern_len) == 0) {
					Genode::print(out, _replacement);
					curr += _pattern_len;
				} else {
					Genode::print(out, Char(*curr));
					curr++;
				}
			}
		}
};


struct Sculpt::Pretty : Subst
{
	template <size_t N>
	Pretty(String<N> const &input) : Subst("_", " ", input) { }
};

#endif /* _XML_H_ */
