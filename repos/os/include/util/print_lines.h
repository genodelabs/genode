/*
 * \brief  Utility for safely writing multi-line text
 * \author Norman Feske
 * \date   2014-01-11
 */

/*
 * Copyright (C) 2014 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__OS__PRINT_LINES_H_
#define _INCLUDE__OS__PRINT_LINES_H_

#include <util/string.h>

namespace Genode {

	template <size_t, typename FUNC>
	static inline void print_lines(char const *, size_t, FUNC const &);
}


/**
 * Print multi-line string
 *
 * \param MAX_LINE_LEN  maximum line length, used to dimension a line buffer
 *                      on the stack
 * \param string        character buffer, not necessarily null-terminated
 * \param len           number of characters to print
 * \param func          functor called for each line with 'char const *' as
 *                      argument
 *
 * In situations where a string is supplied by an untrusted client, we cannot
 * simply print the client-provided content as a single string becausewe cannot
 * expect the client to null-terminate the string properly. The 'Log_multiline'
 * class outputs the content line by line while keeping track of the content
 * size.
 *
 * The output stops when reaching the end of the buffer or when a null
 * character is encountered.
 */
template <Genode::size_t MAX_LINE_LEN, typename FUNC>
void Genode::print_lines(char const *string, size_t len, FUNC const &func)
{
	/* skip leading line breaks */
	for (; *string == '\n'; string++);

	/* number of space and tab characters used for indentation */
	size_t const num_indent_chars =
		({
			size_t n = 0;
			for (; string[n] == ' ' || string[n] == '\t'; n++);
			n;
		});

	char const * const first_line = string;

	while (*string && len) {

		/*
		 * Skip indentation if the pattern is the same as for the first line.
		 */
		if (Genode::strcmp(first_line, string, num_indent_chars) == 0)
			string += num_indent_chars;

		size_t const line_len =
			({
				size_t i = 0;
				for (; string[i] && i < len && string[i] != '\n'; i++);

				/* the newline character belongs to the line */
				if (string[i] == '\n')
					i++;

				i;
			});

		if (!line_len)
			break;

		/*
		 * Copy line from (untrusted) caller to local line buffer
		 */
		char line_buf[MAX_LINE_LEN];
		Genode::strncpy(line_buf, string, Genode::min(line_len + 1, sizeof(line_buf)));
		func(line_buf);

		string += line_len;
		len    -= line_len;
	}
}

#endif /* _INCLUDE__OS__PRINT_LINES_H_ */
