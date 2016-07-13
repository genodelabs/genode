/*
 * \brief  Session label utility class
 * \author Emery Hemingway
 * \author Norman Feske
 * \date   2016-07-01
 */

/*
 * Copyright (C) 2016 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__BASE__SESSION_LABEL_H_
#define _INCLUDE__BASE__SESSION_LABEL_H_

#include <base/snprintf.h>
#include <util/arg_string.h>
#include <util/string.h>

namespace Genode { struct Session_label; }

struct Genode::Session_label : String<160>
{
	private:

		typedef String<capacity()> String;

		static char const *_separator()     { return " -> "; }
		static size_t      _separator_len() { return 4; }

	public:

		using String::String;

		Session_label last_element() const
		{
			char const * const full = string();
			size_t const full_len   = strlen(full);

			if (full_len < _separator_len())
				return full;

			for (unsigned i = full_len - _separator_len(); i > 0; --i)
				if (!strcmp(_separator(), full + i, _separator_len()))
					return full + i + _separator_len();

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
			unsigned prefix_len = length() - _separator_len() - 1;
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
		char buf[Session_label::capacity()];
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
		if (!prefix.valid() || prefix == "")
			return Session_label(label.string());

		if (!label.valid() || label == "")
			return Session_label(prefix.string());

		char buf[Session_label::capacity()];
		snprintf(buf, sizeof(buf), "%s -> %s", prefix.string(), label.string());

		return Session_label(Cstring(buf));
	}
}

#endif /* _INCLUDE__BASE__SESSION_LABEL_H_ */
