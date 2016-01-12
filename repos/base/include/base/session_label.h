/*
 * \brief  Session label utility class
 * \author Emery Hemingway
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
	typedef String<capacity()> String;

	using String::String;

	char const *last_element() const
	{
		char const * const full      = string();
		char const * const separator = " -> ";

		size_t const full_len      = strlen(full);
		size_t const separator_len = strlen(separator);

		if (full_len < separator_len)
			return full;

		for (unsigned i = full_len - separator_len; i > 0; --i)
			if (!strcmp(separator, full + i, separator_len))
				return full + i + separator_len;

		return full;
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

		return Session_label(buf);
	}

	/**
	 * Create a compound label in the form of 'prefix -> label'
	 */
	inline Session_label prefixed_label(char const *prefix, char const *label)
	{
		if (!*prefix)
			return Session_label(label);

		if (!*label)
			return Session_label(prefix);

		char buf[Session_label::capacity()];
		snprintf(buf, sizeof(buf), "%s -> %s", prefix, label);

		return Session_label(buf);
	}
}

#endif /* _INCLUDE__BASE__SESSION_LABEL_H_ */
