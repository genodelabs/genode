/*
 * \brief  Libc-component using select test
 * \author Christian Helmuth
 * \date   2017-02-09
 */

/*
 * Copyright (C) 2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <base/log.h>
#include <base/rpc_server.h>
#include <base/signal.h>
#include <os/static_root.h>
#include <libc/component.h>
#include <libc/select.h>
#include <log_session/log_session.h>
#include <timer_session/connection.h>

/* libc includes */
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/select.h>
#include <sys/fcntl.h>


namespace Log {
	using Genode::Static_root;
	using Genode::Log_session;
	using Genode::Signal_handler;

	struct Session_component;
	struct Main;
}


static void die(char const *token) __attribute__((noreturn));
static void die(char const *token)
{
	Genode::error("[", Genode::Cstring(token), "] ", Genode::Cstring(strerror(errno)));
	exit(1);
}


static void use_file_system()
{
	int result = 0;

	int const fd = open("/tmp/blub", O_RDWR | O_NONBLOCK | O_CREAT | O_APPEND);
	if (fd == -1) die("open");

	printf("open returned fd %d\n", fd);

	static char buf[1024];

	result = read(fd, buf, sizeof(buf));
	if (result == -1) die("read");

	printf("read %d bytes\n", result);

	result = write(fd, "X", 1);
	if (result == -1) die("write");

	printf("wrote %d bytes\n", result);

	result = close(fd);
	if (result == -1) die("close");
}


struct Log::Session_component : Genode::Rpc_object<Log_session>
{
	Libc::Env         &_env;
	Timer::Connection  _timer { _env };

	Signal_handler<Log::Session_component> _timer_handler {
		_env.ep(), *this, &Session_component::_handle_timer };

	void _handle_timer()
	{
		if (_in_read)
			Genode::error("timer fired during read?");
	}

	char   _buf[Log_session::MAX_STRING_LEN];
	int    _fd = -1;
	fd_set _readfds;
	fd_set _writefds;
	fd_set _exceptfds;
	bool   _in_read = false;

	void _select()
	{
		int nready = 0;
		do {
			fd_set readfds   = _readfds;
			fd_set writefds  = _writefds;
			fd_set exceptfds = _exceptfds;

			nready = _select_handler.select(_fd + 1, readfds, writefds, exceptfds);
			if (nready)
				_select_ready(nready, readfds, writefds, exceptfds);
		} while (nready);
	}

	Libc::Select_handler<Session_component> _select_handler {
		*this, &Session_component::_select_ready };

	void _read()
	{
		_in_read = true;

		/* never mind the potentially nested with_libc */
		Libc::with_libc([&] () {
			int const result = read(_fd, _buf, sizeof(_buf)-1);
			if (result <= 0) {
				Genode::warning("read returned ", result, " in select handler");
				return;
			}
			_buf[result] = 0;
			Genode::log("read from file \"", Genode::Cstring(_buf), "\"");
		});

		_in_read = false;
	}

	void _select_ready(int nready, fd_set const &readfds, fd_set const &writefds, fd_set const &exceptfds)
	{
		Libc::with_libc([&] () {
			if (nready <= 0) {
				Genode::warning("select handler reported nready=", nready);
				return;
			}
			if (!FD_ISSET(_fd, &readfds)) {
				Genode::warning("select handler reported unexpected fd, nready=", nready);

				for (unsigned i = 0; i < FD_SETSIZE; ++i)
					if (FD_ISSET(i, &readfds))   Genode::log("fd ", i, " readable");
				for (unsigned i = 0; i < FD_SETSIZE; ++i)
					if (FD_ISSET(i, &writefds))  Genode::log("fd ", i, " writeable");
				for (unsigned i = 0; i < FD_SETSIZE; ++i)
					if (FD_ISSET(i, &exceptfds)) Genode::log("fd ", i, " exceptable?");

				return;
			}
			_read();
		});
	}

	Session_component(Libc::Env &env) : _env(env)
	{
		Libc::with_libc([&] () {
			_fd = open("/dev/terminal", O_RDWR);
			if (_fd == -1) die("open");

			FD_ZERO(&_readfds);
			FD_ZERO(&_writefds);
			FD_ZERO(&_exceptfds);
			FD_SET(_fd, &_readfds);
		});

		_timer.sigh(_timer_handler);
		_timer.trigger_periodic(500*1000);

		/* initially call read two times to ensure blocking */
		_read(); _read();
	}

	void write(String const &string_buf) override
	{
		if (!(string_buf.valid_string())) { return; }

		strncpy(_buf, string_buf.string(), sizeof(_buf));
		size_t len = strlen(_buf);

		if (_buf[len-1] == '\n') _buf[len-1] = 0;

		Genode::log("RPC with \"", Genode::Cstring(_buf), "\"");

		_select();
	}
};


struct Log::Main
{
	Libc::Env               &_env;
	Session_component        _session { _env };
	Static_root<Log_session> _root    { _env.ep().manage(_session) };

	Main(Libc::Env &env) : _env(env)
	{
		Libc::with_libc([] () { use_file_system(); });

		_env.parent().announce(_env.ep().manage(_root));
	}
};


void Libc::Component::construct(Libc::Env &env) { static Log::Main inst(env); }
