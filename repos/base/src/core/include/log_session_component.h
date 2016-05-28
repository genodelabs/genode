/*
 * \brief  Log output service for Core
 * \author Norman Feske
 * \date   2006-09-15
 */

/*
 * Copyright (C) 2006-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _CORE__INCLUDE__LOG_SESSION_COMPONENT_H_
#define _CORE__INCLUDE__LOG_SESSION_COMPONENT_H_

#include <util/string.h>
#include <base/printf.h>
#include <base/rpc_server.h>
#include <log_session/log_session.h>

namespace Genode {

	class Log_session_component : public Rpc_object<Log_session>
	{
		public:

			enum { LABEL_LEN = 64 };

		private:

			char _label[LABEL_LEN];

		public:

			/**
			 * Constructor
			 */
			Log_session_component(const char *label) {
				strncpy(_label, label, sizeof(_label)); }


			/*****************
			 ** Log session **
			 *****************/

			size_t write(String const &string_buf)
			{
				if (!(string_buf.valid_string())) {
					PERR("corrupted string");
					return 0;
				}

				char const *string = string_buf.string();
				size_t len = strlen(string);

				/*
				 * Heuristic: The Log console implementation flushes
				 *            the output preferably in front of escape
				 *            sequences. If the line contains only
				 *            the escape sequence, we skip the printing
				 *            of the label and cut the line break (last
				 *            character).
				 */
				enum { ESC = 27 };
				if ((string[0] == ESC) && (len == 5) && (string[4] == '\n')) {
					char buf[5];
					strncpy(buf, string, 5);
					printf("%s", buf);
					return len;
				}

				char buf[string_buf.MAX_SIZE];
				unsigned from_i = 0;

				for (unsigned i = 0; i < len; i++) {
					if (string[i] == '\n') {
						memcpy(buf, string + from_i, i - from_i);
						buf[i - from_i] = 0;
						printf("[%s] %s\n", _label, buf);
						from_i = i + 1;
					}
				}

				/* if last character of string was not a line break, add one */
				if (from_i < len)
					printf("[%s] %s\n", _label, string + from_i);

				return len;
			}
	};
}

#endif /* _CORE__INCLUDE__LOG_SESSION_COMPONENT_H_ */
