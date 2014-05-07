/*
 * \brief  Convenience functions for operating on a terminal session
 * \author Norman Feske
 * \date   2013-03-19
 */

#ifndef _TERMINAL_UTIL_H_
#define _TERMINAL_UTIL_H_

/* Genode includes */
#include <terminal_session/terminal_session.h>
#include <base/snprintf.h>

namespace Terminal {

	static inline void tprintf(Session &terminal, const char *format_args, ...)
	{
		using namespace Genode;

		enum { MAX_LEN = 256 };
		char buf[MAX_LEN];

		/* process format string */
		va_list list;
		va_start(list, format_args);

		String_console sc(buf, MAX_LEN);
		sc.vprintf(format_args, list);

		va_end(list);

		terminal.write(buf, strlen(buf));
	}
}

#endif /* _TERMINAL_UTIL_H_ */
