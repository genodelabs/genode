/*
 * \brief  Kernel backend for core log messages
 * \author Norman Feske
 * \author Stefan Kalkowski
 * \date   2016-10-10
 */

/*
 * Copyright (C) 2016-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <core_log.h>

static Genode::Core_log_range range { 0, 0 };
static unsigned range_pos   { 0 };

static void out_mem(char const c)
{
	struct Log_memory
	{
		struct Header { unsigned value; } pos;
		char data[1];

		unsigned out(char const c, unsigned cur_pos,
		             Genode::Core_log_range const &range)
		{
			pos.value       = cur_pos;
			data[cur_pos++] = c;
			return cur_pos % (range.size - sizeof(Log_memory::Header));
		}
	} __attribute__((packed)) * mem = reinterpret_cast<Log_memory *>(range.start);

	if (!mem)
		return;

	range_pos = mem->out(c, range_pos, range);
}


void Genode::init_core_log(Core_log_range const &r) { range = r; }


void Genode::Core_log::output(char const * str)
{
	for (unsigned i = 0; i < Genode::strlen(str); i++) {
		out(str[i]);
		out_mem(str[i]);
	}
}
