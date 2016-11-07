/*
 * \brief  Access to the core's log facility
 * \author Norman Feske
 * \author Stefan Kalkowski
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
#include <base/lock.h>

/* base-internal includes */
#include <base/internal/globals.h>
#include <base/internal/output.h>
#include <base/internal/unmanaged_singleton.h>

/* core includes */
#include <core_log.h>

using namespace Genode;

static Log *log_ptr;


Log &Log::log() { return *log_ptr; }


void Genode::init_log()
{
	/* ignore subsequent calls */
	if (log_ptr) return;

	struct Write_fn
	{
		Lock     lock;
		Core_log log;

		void operator () (char const *s)
		{
			Lock::Guard guard(lock);
			log.output(s);
		}
	};

	typedef Buffered_output<512, Write_fn> Buffered_log_output;

	static Buffered_log_output *buffered_log_output =
		unmanaged_singleton<Buffered_log_output>(Write_fn());

	log_ptr = unmanaged_singleton<Log>(*buffered_log_output);
}
