/*
 * \brief  Utilities for formatting output to terminal
 * \author Norman Feske
 * \date   2013-10-05
 */

/*
 * Copyright (C) 2013-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _FORMAT_UTIL_H_
#define _FORMAT_UTIL_H_

/* local includes */
#include <terminal_util.h>

namespace Cli_monitor {

	/**
	 * Print rational number with two fractional decimals
	 */
	static inline size_t format_number(char *dst, size_t len, size_t const value,
	                                   size_t const quotient, char const *unit)
	{
		size_t const integer   = value / quotient;
		size_t const n         = snprintf(dst, len, "%ld.", integer);
		size_t const remainder = ((value - (integer * quotient))*100) / quotient;
	
		if (len == n) return n;
	
		return n + snprintf(dst + n, len - n, "%s%ld%s",
		                    remainder < 10 ? "0" : "", remainder, unit);
	}
	
	
	/**
	 * Print number of bytes using the best suitable unit
	 */
	static inline size_t format_bytes(char *dst, size_t len, size_t bytes)
	{
		enum { KB = 1024, MB = 1024*KB };
	
		if (bytes > MB)
			return format_number(dst, len, bytes, MB, " MiB");
	
		if (bytes > KB)
			return format_number(dst, len, bytes, KB, " KiB");
	
		return snprintf(dst, len, "%ld bytes", bytes);
	}
	
	
	/**
	 * Print number in MiB, without unit
	 */
	static inline size_t format_mib(char *dst, size_t len, size_t bytes)
	{
		enum { KB = 1024, MB = 1024*KB };
	
		return format_number(dst, len, bytes, MB , "");
	}
	
	
	static inline size_t format_bytes(size_t bytes)
	{
		char buf[128];
		return format_bytes(buf, sizeof(buf), bytes);
	}
	
	
	static inline size_t format_mib(size_t bytes)
	{
		char buf[128];
		return format_mib(buf, sizeof(buf), bytes);
	}
	
	
	static inline void tprint_bytes(Terminal::Session &terminal, size_t bytes)
	{
		char buf[128];
		format_bytes(buf, sizeof(buf), bytes);
		Terminal::tprintf(terminal, "%s", buf);
	}
	
	
	static inline void tprint_mib(Terminal::Session &terminal, size_t bytes)
	{
		char buf[128];
		format_mib(buf, sizeof(buf), bytes);
		Terminal::tprintf(terminal, "%s", buf);
	}
	
	
	static inline void tprint_status_bytes(Terminal::Session &terminal,
	                                       char const *label, size_t bytes)
	{
		Terminal::tprintf(terminal, label);
		tprint_bytes(terminal, bytes);
		Terminal::tprintf(terminal, "\n");
	}
	
	
	static void tprint_padding(Terminal::Session &terminal, size_t pad, char c = ' ')
	{
		char const buf[2] = { c, 0 };
		for (unsigned i = 0; i < pad; i++)
			Terminal::tprintf(terminal, buf);
	}
}

#endif /* _FORMAT_UTIL_H_ */
