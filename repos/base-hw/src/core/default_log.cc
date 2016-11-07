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

/* base-internal includes */
#include <base/internal/globals.h>
#include <base/internal/output.h>
#include <base/internal/unmanaged_singleton.h>

#include <core_log.h>
#include <serial.h>
#include <kernel/log.h>

using namespace Genode;

static Log *log_ptr;


Log &Log::log() { return *log_ptr; }


static void out_char(char const c)
{
	enum {
		ASCII_LINE_FEED = 10,
		ASCII_CARRIAGE_RETURN = 13,
		BAUD_RATE = 115200
	};

	Serial & serial = *unmanaged_singleton<Serial>(BAUD_RATE);
	if (c == ASCII_LINE_FEED) serial.put_char(ASCII_CARRIAGE_RETURN);
	serial.put_char(c);
}


void Genode::Core_log::out(char const c) { out_char(c); }


void Genode::init_log()
{
	/* ignore subsequent calls */
	if (log_ptr) return;

	struct Write_fn
	{
		Core_log log;
		void operator () (char const *s) { log.output(s); }
	};

	typedef Buffered_output<512, Write_fn> Buffered_log_output;

	static Buffered_log_output *buffered_log_output =
		unmanaged_singleton<Buffered_log_output>(Write_fn());

	log_ptr = unmanaged_singleton<Log>(*buffered_log_output);
}


void Kernel::log(char const c) { out_char(c); }
