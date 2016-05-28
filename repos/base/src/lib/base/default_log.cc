/*
 * \brief  Access to the component's LOG session
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
#include <log_session/log_session.h>

/* base-internal includes */
#include <base/internal/globals.h>
#include <base/internal/output.h>
#include <base/internal/unmanaged_singleton.h>

using namespace Genode;


static Log *log_ptr;


Log &Log::log() { return *log_ptr; }


extern "C" int stdout_write(const char *s);


void Genode::init_log()
{
	/* ignore subsequent calls */
	if (log_ptr)
		return;

	struct Write_fn { void operator () (char const *s) { stdout_write(s); } };

	typedef Buffered_output<Log_session::MAX_STRING_LEN, Write_fn>
	        Buffered_log_output;

	static Buffered_log_output *buffered_log_output =
		unmanaged_singleton<Buffered_log_output>(Write_fn());

	log_ptr = unmanaged_singleton<Log>(*buffered_log_output);
}

