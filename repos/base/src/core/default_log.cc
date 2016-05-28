/*
 * \brief  Access to the core's log facility
 * \author Norman Feske
 * \date   2016-05-03
 */

/*
 * Copyright (C) 2016 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <base/log.h>
#include <base/printf.h>

/* base-internal includes */
#include <base/internal/globals.h>
#include <base/internal/output.h>
#include <base/internal/unmanaged_singleton.h>

using namespace Genode;


static Log *log_ptr;


Log &Log::log() { return *log_ptr; }


void Genode::init_log()
{
	/* ignore subsequent calls */
	if (log_ptr)
		return;

	/*
	 * Currently, we still rely on the old format-string support and
	 * produce output via 'core_printf.cc'.
	 */
	struct Write_fn { void operator () (char const *s) { printf("%s", s); } };

	typedef Buffered_output<512, Write_fn> Buffered_log_output;

	static Buffered_log_output *buffered_log_output =
		unmanaged_singleton<Buffered_log_output>(Write_fn());

	log_ptr = unmanaged_singleton<Log>(*buffered_log_output);
}

