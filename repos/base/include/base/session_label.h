/*
 * \brief  Session label utility class
 * \author Emery Hemingway
 * \author Norman Feske
 * \date   2016-07-01
 */

/*
 * Copyright (C) 2016-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__BASE__SESSION_LABEL_H_
#define _INCLUDE__BASE__SESSION_LABEL_H_

#include <util/arg_string.h>
#include <util/string.h>

namespace Genode { struct Session_label; }

struct Genode::Session_label : String<160>
{
	private:

		static char const *_separator()     { return " -> "; }
		static size_t      _separator_len() { return 4; }

	public:

		using String = String<capacity()>;
		using String::String;

		/**
		 * Copy constructor
		 *
		 * This constructor is needed because GCC 8 disregards derived
		 * copy constructors as candidate.
		 */
		template <size_t N>
		Session_label(Genode::String<N> const &other) : Genode::String<160>(other) { }

		Session_label last_element() const
		{
			char const * const full = string();
			size_t const full_len   = strlen(full);

			if (full_len < _separator_len())
				return full;

			size_t i = full_len - _separator_len();
			do {
				if (!strcmp(_separator(), full + i, _separator_len()))
					return full + i + _separator_len();
			} while (i-- > 0);

			return Session_label(Cstring(full));
		}

		/**
		 * Return part of the label without the last element
		 */
		inline Session_label prefix() const
		{
			if (length() < _separator_len() + 1)
				return Session_label();

			/* search for last occurrence of the separator */
			size_t prefix_len = length() - _separator_len() - 1;
			char const * const full = string();

			for (; prefix_len > 0; prefix_len--)
				if (strcmp(full + prefix_len, _separator(), _separator_len()) == 0)
					break;

			/* construct new label with only the prefix part */
			return Session_label(Cstring(full, prefix_len));
		}
};


namespace Genode {

	/**
	 * Extract label from session arguments in the form of 'label="..."'
	 */
	inline Session_label label_from_args(char const *args)
	{
		char buf[Session_label::capacity()] { };
		Arg_string::find_arg(args, "label").string(buf, sizeof(buf), "");

		return Session_label(Cstring(buf));
	}

	/**
	 * Create a compound label in the form of 'prefix -> label'
	 */
	template <size_t N1, size_t N2>
	inline Session_label prefixed_label(String<N1> const &prefix,
	                                    String<N2> const &label)
	{
		String<N1 + N2 + 4> const prefixed_label(prefix, " -> ", label);
		return Session_label(prefixed_label);
	}
}

#endif /* _INCLUDE__BASE__SESSION_LABEL_H_ */
