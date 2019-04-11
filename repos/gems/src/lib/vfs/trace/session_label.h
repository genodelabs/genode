/*
 * \brief  Session label extension
 * \author Sebastian Sumpf
 * \date   2019-06-04
 */

/*
 * Copyright (C) 2019 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _SESSION_LABEL_H_
#define _SESSION_LABEL_H_

#include <base/snprintf.h>
#include <util/arg_string.h>
#include <util/string.h>

namespace Vfs {
using namespace Genode;
struct Session_label;
}

struct Vfs::Session_label : Genode::Session_label
{
	private:

		static char const *_separator()     { return " -> "; }
		static size_t      _separator_len() { return 4; }

	public:

		using Genode::Session_label::Session_label;

		Session_label first_element() const
		{
			char const * const full = string();
			if (length() < _separator_len() + 1)
				return Session_label(Cstring(full));

			unsigned prefix_len;

			for (prefix_len = 0; prefix_len < length() - 1; prefix_len++)
				if (!strcmp(_separator(), full + prefix_len, _separator_len()))
					break;

			return Session_label(Cstring(full, prefix_len));
		}

		/**
		 * Return part of the label without first element
		 */
		Session_label suffix() const
		{
			if (length() < _separator_len() + 1)
				return Session_label();

			char const * const full = string();
			for (unsigned prefix_len = 0; prefix_len < length() - 1; prefix_len++)
				if (!strcmp(_separator(), full + prefix_len, _separator_len()))
					return full + prefix_len + _separator_len();

			return Session_label();
		}
};

#endif /* _SESSION_LABEL_H_ */
