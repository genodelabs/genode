/*
 * \brief  Log output service for Core
 * \author Norman Feske
 * \date   2006-09-15
 */

/*
 * Copyright (C) 2006-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _CORE__INCLUDE__LOG_SESSION_COMPONENT_H_
#define _CORE__INCLUDE__LOG_SESSION_COMPONENT_H_

#include <base/rpc_server.h>
#include <base/session_label.h>
#include <log_session/log_session.h>

/* core includes */
#include <types.h>

namespace Core { class Log_session_component; }


class Core::Log_session_component : public Rpc_object<Log_session>
{
	private:

		Session_label const _label;

	public:

		/**
		 * Constructor
		 */
		Log_session_component(Session_label const &label)
		: _label("[", label, "] ") { }


		/*****************
		 ** Log session **
		 *****************/

		void write(String const &string_buf) override
		{
			if (!(string_buf.valid_string())) {
				error("corrupted string");
				return;
			}

			char const * const string = string_buf.string();
			size_t const len = Genode::strlen(string);

			unsigned from_i = 0;
			for (unsigned i = 0; i < len; i++) {
				if (string[i] == '\n') {
					log(_label, Cstring(string + from_i, i - from_i));
					from_i = i + 1;
				}
			}

			/* if last character of string was not a line break, add one */
			if (from_i < len)
				log(_label, Cstring(string + from_i));
		}
};

#endif /* _CORE__INCLUDE__LOG_SESSION_COMPONENT_H_ */
