/*
 * \brief  Parsing utilities
 * \author Norman Feske
 * \date   2023-06-06
 */

/*
 * Copyright (C) 2023 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _MONITOR__STRING_H_
#define _MONITOR__STRING_H_

#include <util/string.h>

namespace Genode {

	static void with_max_bytes(Const_byte_range_ptr const &bytes,
	                           size_t const max, auto const &fn)
	{
		fn(Const_byte_range_ptr { bytes.start, min(max, bytes.num_bytes) });
	}

	static void with_skipped_bytes(Const_byte_range_ptr const &bytes,
	                               size_t const n, auto const &fn)
	{
		if (bytes.num_bytes < n)
			return;

		Const_byte_range_ptr const remainder { bytes.start     + n,
		                                       bytes.num_bytes - n };
		fn(remainder);
	}

	static void with_skipped_prefix(Const_byte_range_ptr const &bytes,
	                                Const_byte_range_ptr const &prefix,
	                                auto const &fn)
	{
		if (bytes.num_bytes < prefix.num_bytes)
			return;

		if (strcmp(prefix.start, bytes.start, prefix.num_bytes) != 0)
			return;

		with_skipped_bytes(bytes, prefix.num_bytes, fn);
	}

	static void with_skipped_prefix(Const_byte_range_ptr const &bytes,
	                                char const *prefix, auto const &fn)
	{
		with_skipped_prefix(bytes, Const_byte_range_ptr { prefix, strlen(prefix) }, fn);
	}

	template <size_t N>
	static void with_skipped_prefix(Const_byte_range_ptr const &bytes,
	                                String<N> const &prefix, auto const &fn)
	{
		with_skipped_prefix(bytes, prefix.string(), fn);
	}

	/**
	 * Return true if 'bytes' equals the null-terminated string 'str'
	 */
	static inline bool equal(Const_byte_range_ptr const &bytes, char const *str)
	{
		size_t const len = strlen(str);

		return (len == bytes.num_bytes) && (strcmp(bytes.start, str, len) == 0);
	}

}

#endif /* _MONITOR__STRING_H_ */
