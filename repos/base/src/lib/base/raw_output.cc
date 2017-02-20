/*
 * \brief  Access to raw output back end
 * \author Norman Feske
 * \date   2016-06-16
 */

/*
 * Copyright (C) 2016-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <base/log.h>

/* base-internal includes */
#include <base/internal/unmanaged_singleton.h>
#include <base/internal/output.h>
#include <base/internal/raw_write_string.h>


Genode::Output &Genode::Raw::_output()
{
	struct Write_fn { void operator () (char const *s) { raw_write_string(s); } };

	typedef Buffered_output<256, Write_fn> Buffered_raw_output;

	static Buffered_raw_output *buffered_raw_output =
		unmanaged_singleton<Buffered_raw_output>(Write_fn());

	return *buffered_raw_output;
}
