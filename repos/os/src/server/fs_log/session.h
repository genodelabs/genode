/*
 * \brief  Log session that writes messages to a file system.
 * \author Emery Hemingway
 * \date   2015-05-16
 *
 * Message writing is fire-and-forget to prevent
 * logging from becoming I/O bound.
 */

/*
 * Copyright (C) 2015 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _FS_LOG__SESSION_H_
#define _FS_LOG__SESSION_H_

/* Genode includes */
#include <log_session/log_session.h>
#include <file_system_session/file_system_session.h>
#include <base/rpc_server.h>

/* Local includes */
#include "log_file.h"

namespace Fs_log {
	class           Session_component;
	class Unlabeled_session_component;
	class   Labeled_session_component;
}

class Fs_log::Session_component : public Rpc_object<Log_session, Unlabeled_session_component>
{
	protected:

		Log_file &_log_file;

	public:

		Session_component(Log_file &log_file)
		: _log_file(log_file) { _log_file.incr(); }

		~Session_component() { _log_file.decr(); }

		Log_file *file() const { return &_log_file; }

		virtual size_t write(String const &string) = 0;
};

class Fs_log::Unlabeled_session_component : public Session_component
{

	public:

		/**
		 * Constructor
		 */
		Unlabeled_session_component(Log_file &log_file)
		: Session_component(log_file) { }

		/*****************
		 ** Log session **
		 *****************/

		size_t write(Log_session::String const &msg)
		{
			if (!msg.is_valid_string()) {
				PERR("corrupted string");
				return 0;
			}

			char const *msg_str = msg.string();
			size_t msg_len = Genode::strlen(msg_str);

			return _log_file.write(msg_str, msg_len);
		}
};

class Fs_log::Labeled_session_component : public Session_component
{
	private:

		char      _label[Log_session::String::MAX_SIZE];
		size_t    _label_len;

	public:

		/**
		 * Constructor
		 */
		Labeled_session_component(char const *label, Log_file &log_file)
		: Session_component(log_file)
		{
			snprintf(_label, sizeof(_label), "[%s] ", label);
			_label_len = strlen(_label);
		}

		/*****************
		 ** Log session **
		 *****************/

		size_t write(Log_session::String const &msg)
		{
			if (!msg.is_valid_string()) {
				PERR("corrupted string");
				return 0;
			}

			char const *msg_str = msg.string();
			size_t msg_len = Genode::strlen(msg_str);

			_log_file.write(_label, _label_len);
			return _log_file.write(msg_str, msg_len);
		}
};

#endif
