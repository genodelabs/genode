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
				if (!(string_buf.is_valid_string())) {
					PERR("corrupted string");
					return 0;
				}

				char const *string = string_buf.string();
				int len = strlen(string);

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

				printf("[%s] %s", _label, string);

				/* if last character of string was not a line break, add one */
				if ((len > 0) && (string[len - 1] != '\n'))
					printf("\n");

				return len;
			}
	};
}

#endif /* _CORE__INCLUDE__LOG_SESSION_COMPONENT_H_ */
