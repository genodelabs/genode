/*
 * Copyright (C) 2016-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <stdarg.h>
#include <log_session/connection.h>
#include <format/console.h>

#include "util.h"

using namespace Genode;


struct Formatted_log : Format::Console
{
	Log_connection con;

	char str[Log_session::MAX_STRING_LEN] { };

	size_t pos { };

	Formatted_log(Env &env) : con(env, "debug") { }

	void _out_char(char c) override
	{
		if (c != '\n')
			str[pos++] = c;

		if (c == '\n' || pos == sizeof(str)) {
			str[pos] = 0;
			con.write(str);
			pos = 0;
		}
	}
};


static Formatted_log *formatted_log;


void Acpica::init_printf(Env &env)
{
	static Formatted_log instance(env);

	formatted_log = &instance;
};


extern "C" {

#include "acpi.h"
#include "acpiosxf.h"

void AcpiOsVprintf (const char *fmt, va_list va)
{
	if (formatted_log)
		formatted_log->vprintf(fmt, va);
}


void AcpiOsPrintf (const char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	AcpiOsVprintf(fmt, args);
	va_end(args);
}

}
