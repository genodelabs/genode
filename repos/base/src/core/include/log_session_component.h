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
#include <base/log.h>
#include <base/rpc_server.h>
#include <log_session/log_session.h>

namespace Genode {

	class Log_session_component : public Rpc_object<Log_session>
	{
		public:

			enum { LABEL_LEN = 128 };

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
					error("corrupted string");
					return 0;
				}

				char const *string = string_buf.string();
				size_t len = strlen(string);

				char buf[string_buf.MAX_SIZE];
				unsigned from_i = 0;

				for (unsigned i = 0; i < len; i++) {
					if (string[i] == '\n') {
						memcpy(buf, string + from_i, i - from_i);
						buf[i - from_i] = 0;
						log("[", Cstring(_label), "] ", Cstring(buf));
						from_i = i + 1;
					}
				}

				/* if last character of string was not a line break, add one */
				if (from_i < len)
					log("[", Cstring(_label), "] ", Cstring(string + from_i));

				return len;
			}
	};
}

#endif /* _CORE__INCLUDE__LOG_SESSION_COMPONENT_H_ */
