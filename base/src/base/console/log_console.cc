/*
 * \brief  Printf backend for the LOG interface
 * \author Norman Feske
 * \date   2006-09-15
 */

/*
 * Copyright (C) 2006-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#include <log_session/connection.h>
#include <base/printf.h>
#include <base/console.h>
#include <base/lock.h>
#include <base/env.h>

using namespace Genode;


void *operator new (size_t, void *ptr) { return ptr; }


class Log_console : public Console
{
	private:

		enum { _BUF_SIZE = 216 };

		Log_connection _log;
		char           _buf[_BUF_SIZE];
		unsigned       _num_chars;
		Lock           _lock;

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
		Log_session *log_session() { return &_log; }

		/**
		 * Re-establish LOG session
		 */
		void reconnect()
		{
			/*
			 * Note that the destructor of old 'Log_connection' is not called.
			 * This is not needed because the only designated use of this
			 * function is the startup procedure of noux processes created
			 * via fork. At the point of calling this function, the new child
			 * has no valid capability to the original LOG session anyway.
			 */
			new (&_log) Log_connection;
		}
};


/*
 * In the presence of a libC, we use the libC's full printf implementation and
 * use the 'Log_console' as backend.
 */

Log_console *stdout_log_console()
{
	/*
	 * Construct the log console object on the first call of this function.
	 * In constrast to having a static variable in the global scope, the
	 * constructor gets only called when needed and no static constructor
	 * gets invoked by the initialization code.
	 */
	static Log_console static_log_console;

	return &static_log_console;
}


/**
 * Hook for supporting libc back ends for stdio
 */
extern "C" int stdout_write(const char *s)
{
	return stdout_log_console()->log_session()->write(s);
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
