/*
 * \brief  Printf backend for the LOG interface
 * \author Norman Feske
 * \date   2006-09-15
 */

/*
 * Copyright (C) 2006-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <log_session/connection.h>
#include <base/printf.h>
#include <base/console.h>
#include <base/lock.h>
#include <base/env.h>
#include <base/internal/unmanaged_singleton.h>

using namespace Genode;

class Log_console : public Console
{
	private:

		enum { _BUF_SIZE = Log_session::MAX_STRING_LEN };


		struct Log : Log_session_client
		{
			Session_capability _cap() {
				return env_deprecated()->parent()->session_cap(Parent::Env::log()); }

			Log() : Log_session_client(reinterpret_cap_cast<Log_session>(_cap()))
			{ }
		};

		Log _log;

		char     _buf[_BUF_SIZE];
		unsigned _num_chars;
		Lock     _lock;

		void _flush()
		{
			/* null-terminate string */
			_buf[_num_chars] = 0;
			_log.write(_buf);

			/* restart with empty buffer */
			_num_chars = 0;
		}

	protected:

		void _out_char(char c)
		{
			/* ensure enough buffer space for complete escape sequence */
			if ((c == 27) && (_num_chars + 8 > _BUF_SIZE)) _flush();

			_buf[_num_chars++] = c;

			/* flush immediately on line break */
			if (c == '\n' || _num_chars >= sizeof(_buf) - 1)
                          _flush();
		}

	public:

		/**
		 * Constructor
		 */
		Log_console()
		:
			_num_chars(0)
		{ }

		/**
		 * Console interface
		 */
		void vprintf(const char *format, va_list list)
		{
			Lock::Guard lock_guard(_lock);
			Console::vprintf(format, list);
		}

		/**
		 * Return LOG session interface
		 */
		Log_session &log_session() { return _log; }

		/**
		 * Re-establish LOG session
		 */
		void reconnect()
		{
			/*
			 * We cannot use a 'Reconstructible' because we have to skip
			 * the object destruction inside a freshly forked process.
			 * Otherwise, the attempt to destruct the capability contained
			 * in the 'Log' object would result in an inconsistent ref counter
			 * of the respective capability-space element.
			 */
			construct_at<Log>(&_log);
		}
};


/*
 * In the presence of a libC, we use the libC's full printf implementation and
 * use the 'Log_console' as backend.
 */

static Log_console *stdout_log_console() { return unmanaged_singleton<Log_console>(); }


/**
 * Hook for supporting libc back ends for stdio
 */
extern "C" int stdout_write(const char *s)
{
	return stdout_log_console()->log_session().write(s);
}


/**
 * Hook for support the 'fork' implementation of the noux libc backend
 */
extern "C" void stdout_reconnect() { stdout_log_console()->reconnect(); }


void Genode::printf(const char *format, ...)
{
	va_list list;
	va_start(list, format);

	vprintf(format, list);

	va_end(list);
}


void Genode::vprintf(const char *format, va_list list)
{
	stdout_log_console()->vprintf(format, list);
}
