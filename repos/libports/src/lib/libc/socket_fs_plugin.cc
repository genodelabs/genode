/*
 * \brief  Libc pseudo plugin for socket fs
 * \author Christian Helmuth
 * \author Christian Prochaska
 * \author Norman Feske
 * \author Emery Hemingway
 * \date   2015-06-23
 */

/*
 * Copyright (C) 2015-2019 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <base/env.h>
#include <base/log.h>
#include <vfs/types.h>
#include <util/string.h>
#include <libc/allocator.h>

/* libc includes */
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <ctype.h>
#include <netinet/tcp.h>
#include <stdio.h>
#include <sys/ioctl.h>
#include <ifaddrs.h>
#include <net/if.h>

/* libc-internal includes */
#include <internal/kernel.h>
#include <internal/socket_fs_plugin.h>
#include <internal/file.h>
#include <internal/errno.h>
#include <internal/init.h>
#include <internal/suspend.h>
#include <internal/pthread.h>


namespace Libc {
	extern char const *config_socket();
	bool read_ready_from_kernel(File_descriptor *);
	bool write_ready_from_kernel(File_descriptor *);
}


static Libc::Suspend *_suspend_ptr;
static Libc::Monitor *_monitor_ptr;


void Libc::init_socket_fs(Suspend &suspend, Monitor &monitor)
{
	_suspend_ptr = &suspend;
	_monitor_ptr = &monitor;
}


static Libc::Monitor & monitor()
{
	struct Missing_call_of_init_socket_fs : Genode::Exception { };
	if (!_monitor_ptr)
		throw Missing_call_of_init_socket_fs();
	return *_monitor_ptr;
}


namespace { using Fn = Libc::Monitor::Function_result; }


/***************
 ** Utilities **
 ***************/

namespace Libc { namespace Socket_fs {

	struct Absolute_path : Vfs::Absolute_path
	{
		Absolute_path() { }

		Absolute_path(char const *path, char const *pwd = 0)
		:
			Vfs::Absolute_path(path, pwd)
		{
			remove_trailing('\n');
		}
	};

	template <int> class String;
	typedef String<NI_MAXHOST> Host_string;
	typedef String<NI_MAXSERV> Port_string;
	struct Sockaddr_string;

	struct New_socket_failed : Exception { };
	struct Address_conversion_failed : Exception { };

	struct Context;
	struct Plugin;
	struct Sockaddr_functor;
	struct Remote_functor;
	struct Local_functor;

	Plugin & plugin();

	enum { MAX_CONTROL_PATH_LEN = 16 };
} }


using namespace Libc::Socket_fs;


struct Libc::Socket_fs::Context : Plugin_context
{
	public:

		enum Proto { TCP, UDP };

		enum State { UNCONNECTED, ACCEPT_ONLY, CONNECTING, CONNECTED, CONNECT_ABORTED, UNSPEC };

		/* TODO remove */
		struct Inaccessible { }; /* exception */

	private:

		/*
		 * This is the file descriptor representing the socket and must be held
		 * open until the socket is closed. The file content is the location of
		 * the socket in the socket FS.
		 */
		int const _handle_fd;
		int       _fd_flags { 0 };

		Absolute_path _read_socket_path()
		{
			Absolute_path path;
			int const n = read(_handle_fd, path.base(),
			                   Absolute_path::capacity()-1);
			if (n == -1 || !n || n >= (int)Absolute_path::capacity() - 1)
				throw New_socket_failed();
			*(path.base()+n) = '\0';

			return path;
		}

		Absolute_path const _path {
			_read_socket_path().base(), config_socket() };

		enum Fd { DATA, PEEK, CONNECT, BIND, LISTEN, ACCEPT, LOCAL, REMOTE, MAX };

		struct
		{
			char const      *name;
			int              num;
			File_descriptor *file;
		} _fd[Fd::MAX] = {
			{ "data",    -1, nullptr }, { "peek",   -1, nullptr },
			{ "connect", -1, nullptr }, { "bind",   -1, nullptr },
			{ "listen",  -1, nullptr }, { "accept", -1, nullptr },
			{ "local",   -1, nullptr }, { "remote", -1, nullptr }
		};


		Proto const _proto;

		State _state { UNCONNECTED };

		template <typename FUNC>
		void _fd_apply(FUNC const &fn)
		{
			for (unsigned i = 0; i < Fd::MAX; ++i)
				if (_fd[i].num != -1) fn(_fd[i].num);
		}

		void _init_fd(Fd type, int flags)
		{
			Absolute_path file(_fd[type].name, _path.base());
			int const fd = open(file.base(), flags|_fd_flags);
			if (fd == -1) {
				error(__func__, ": ", _fd[type].name,
				      " file not accessible at ", file,
				      " errno=", errno);
				throw New_socket_failed();
			}
			_fd[type].num  = fd;
			_fd[type].file = file_descriptor_allocator()->find_by_libc_fd(fd);
		}

		bool _fd_read_ready(Fd type)
		{
			if (_fd[type].file)
				return Libc::read_ready_from_kernel(_fd[type].file);
			else
				return false;
		}

		bool _fd_write_ready(Fd type)
		{
			if (_fd[type].file)
				return Libc::write_ready_from_kernel(_fd[type].file);
			else
				return false;
		}

	public:

		Context(Proto proto, int handle_fd)
		: _handle_fd(handle_fd), _proto(proto)
		{
			_init_fd(Fd::DATA,    O_RDWR);
			_init_fd(Fd::PEEK,    O_RDONLY);
			_init_fd(Fd::CONNECT, O_RDWR);
			_init_fd(Fd::BIND,    O_WRONLY);
			_init_fd(Fd::LISTEN,  O_WRONLY);
			_init_fd(Fd::ACCEPT,  O_RDONLY);
			_init_fd(Fd::LOCAL,   O_RDWR);
			_init_fd(Fd::REMOTE,  O_RDWR);
		}

		~Context()
		{
			_fd_apply([] (int fd) { ::close(fd); });
			::close(_handle_fd);
		}

		Absolute_path path() const { return _path; }

		Proto proto() const { return _proto; }

		int fd_flags() const { return _fd_flags; }
		void fd_flags(int flags)
		{
			_fd_flags = flags;
			_fd_apply([flags] (int fd) { fcntl(fd, F_SETFL, flags); });
		}

		int data_fd()    { return _fd[Fd::DATA].num; }
		int peek_fd()    { return _fd[Fd::PEEK].num; }
		int connect_fd() { return _fd[Fd::CONNECT].num; }
		int bind_fd()    { return _fd[Fd::BIND].num; }
		int listen_fd()  { return _fd[Fd::LISTEN].num; }
		int accept_fd()  { return _fd[Fd::ACCEPT].num; }
		int local_fd()   { return _fd[Fd::LOCAL].num; }
		int remote_fd()  { return _fd[Fd::REMOTE].num; }

		/* request the appropriate fd to ensure the file is open */
		bool connect_read_ready() { return _fd_read_ready(Fd::CONNECT); }
		bool data_read_ready()    { return _fd_read_ready(Fd::DATA); }
		bool accept_read_ready()  { return _fd_read_ready(Fd::ACCEPT); }
		bool local_read_ready()   { return _fd_read_ready(Fd::LOCAL); }
		bool remote_read_ready()  { return _fd_read_ready(Fd::REMOTE); }

		void state(State state) { _state = state; }
		State state() const     { return _state; }

		bool read_ready()
		{
			return (_state == ACCEPT_ONLY) ? accept_read_ready() : data_read_ready();
		}

		bool write_ready()
		{
			if (_state == CONNECTING)
				return connect_read_ready();

			return _fd_write_ready(Fd::DATA);
		}

		/*
		 * Read the connect status from the connect file and return 0 if connected
		 * or -1 with errno set to the error code.
		 */
		int read_connect_status()
		{
			char connect_status[32] = { 0 };
			ssize_t connect_status_len;

			connect_status_len = read(connect_fd(), connect_status,
			                          sizeof(connect_status));

			if (connect_status_len <= 0) {
				error("socket_fs: reading from the connect file failed");
				return -1;
			}

			using ::strcmp;

			if (strcmp(connect_status, "connected") == 0)
				return 0;

			if (strcmp(connect_status, "connection refused") == 0)
				return Errno(ECONNREFUSED);

			if (strcmp(connect_status, "not connected") == 0)
				return Errno(ENOTCONN);

			if (strcmp(connect_status, "no route to host") == 0)
				return Errno(EHOSTUNREACH);

			error("socket_fs: unhandled connection state");
			return Errno(ECONNREFUSED);
		}
};


struct Libc::Socket_fs::Sockaddr_functor : Suspend_functor
{
	Socket_fs::Context &context;
	bool const          nonblocking;

	Sockaddr_functor(Socket_fs::Context &context, bool nonblocking)
	: context(context), nonblocking(nonblocking) { }

	virtual int fd() = 0;
};


struct Libc::Socket_fs::Remote_functor : Sockaddr_functor
{
	Remote_functor(Socket_fs::Context &context, bool nonblocking)
	: Sockaddr_functor(context, nonblocking) { }

	bool suspend() override { return !nonblocking && !context.remote_read_ready(); }
	int fd()       override { return context.remote_fd(); }
};


struct Libc::Socket_fs::Local_functor : Sockaddr_functor
{
	Local_functor(Context &context, bool nonblocking)
	: Sockaddr_functor(context, nonblocking) { }

	bool suspend() override { return !nonblocking && !context.local_read_ready(); }
	int fd()       override { return context.local_fd(); }
};


struct Libc::Socket_fs::Plugin : Libc::Plugin
{
	bool supports_poll() override { return true; }
	bool supports_select(int, fd_set *, fd_set *, fd_set *, timeval *) override;

	ssize_t read(File_descriptor *, void *, ::size_t) override;
	ssize_t write(File_descriptor *, const void *, ::size_t) override;
	int fcntl(File_descriptor *, int, long) override;
	int close(File_descriptor *) override;
	bool poll(File_descriptor &fd, struct pollfd &pfd) override;
	int select(int, fd_set *, fd_set *, fd_set *, timeval *) override;
	int ioctl(File_descriptor *, unsigned long, char *) override;
};


template <int CAPACITY> class Libc::Socket_fs::String
{
	private:

		char _buf[CAPACITY] { 0 };

	public:

		String() { }

		constexpr size_t capacity() { return CAPACITY; }

		char const * base() const { return _buf; }
		char       * base()       { return _buf; }

		void terminate(size_t at) { _buf[at] = 0; }

		void remove_trailing_newline()
		{
			int i = 0;
			while (_buf[i] && _buf[i + 1]) i++;

			if (i > 0 && _buf[i] == '\n')
				_buf[i] = 0;
		}
};

/*
 * Both NI_MAXHOST and NI_MAXSERV include the terminating 0, which allows
 * use to put ':' between host and port on concatenation.
 */
struct Libc::Socket_fs::Sockaddr_string : String<NI_MAXHOST + NI_MAXSERV>
{
	Sockaddr_string() {	stpcpy(base(), ";0"); }

	Sockaddr_string(Host_string const &host, Port_string const &port)
	{
		char *b = base();
		b = stpcpy(b, host.base());
		b = stpcpy(b, ":");
		b = stpcpy(b, port.base());
	}

	Host_string host() const
	{
		Host_string host;

		Genode::copy_cstring(host.base(), base(), host.capacity());
		char *at = strstr(host.base(), ":");
		if (!at)
			throw Address_conversion_failed();
		*at = 0;

		return host;
	}

	Port_string port() const
	{
		Port_string port;

		char *at = strstr(base(), ":");
		if (!at)
			throw Address_conversion_failed();

		Genode::copy_cstring(port.base(), ++at, port.capacity());

		return port;
	}
};


using namespace Libc;


static Port_string port_string(sockaddr_in const &addr)
{
	Port_string port;

	if (getnameinfo((sockaddr *)&addr, sizeof(addr),
	                nullptr, 0, /* no host conversion */
	                port.base(), port.capacity(),
	                NI_NUMERICHOST | NI_NUMERICSERV) != 0)
		throw Address_conversion_failed();

	return port;
}


static Host_string host_string(sockaddr_in const &addr)
{
	Host_string host;

	if (getnameinfo((sockaddr *)&addr, sizeof(addr),
	                host.base(), host.capacity(),
	                nullptr, 0, /* no port conversion */
	                NI_NUMERICHOST | NI_NUMERICSERV) != 0)
		throw Address_conversion_failed();

	return host;
}


static sockaddr_in sockaddr_in_struct(Host_string const &host, Port_string const &port)
{
	addrinfo hints;
	addrinfo *info = nullptr;

	::memset(&hints, 0, sizeof(hints));
	hints.ai_flags = AI_NUMERICHOST | AI_NUMERICSERV;

	if (getaddrinfo(host.base(), port.base(), &hints, &info))
		throw Address_conversion_failed();

	sockaddr_in addr = *(sockaddr_in*)info->ai_addr;

	freeaddrinfo(info);

	return addr;
}


static int read_sockaddr_in(Socket_fs::Sockaddr_functor &func,
                            struct sockaddr_in *addr, socklen_t *addrlen)
{
	if (!addr)                     return Errno(EFAULT);
	if (!addrlen || *addrlen <= 0) return Errno(EINVAL);

	while (!func.nonblocking && func.suspend()) {

		struct Missing_call_of_init_socket_fs : Exception { };
		if (!_suspend_ptr)
			throw Missing_call_of_init_socket_fs();

		_suspend_ptr->suspend(func);
	}

	Sockaddr_string addr_string;
	int const n = read(func.fd(), addr_string.base(), addr_string.capacity() - 1);

	if (n == -1) return Errno(errno);
	/* 0 return value means "no packet resp. not connected" */
	if (!n)
		switch (func.context.proto()) {
		case Socket_fs::Context::Proto::UDP: return Errno(EAGAIN);
		case Socket_fs::Context::Proto::TCP: return Errno(ENOTCONN);
		}
	if (n >= (int)addr_string.capacity() - 1) return Errno(EINVAL);

	addr_string.terminate(n);
	addr_string.remove_trailing_newline();

	try {
		/* convert the address but do not exceed the caller's buffer */
		sockaddr_in saddr = sockaddr_in_struct(addr_string.host(), addr_string.port());
		::memcpy(addr, &saddr, *addrlen);
		*addrlen = sizeof(saddr);

		return 0;
	} catch (Address_conversion_failed) {
		warning("IP address conversion failed");
		return Errno(ENOBUFS);
	}
}


/***********************
 ** Address functions **
 ***********************/

extern "C" int socket_fs_getpeername(int libc_fd, sockaddr *addr, socklen_t *addrlen)
{
	File_descriptor *fd = file_descriptor_allocator()->find_by_libc_fd(libc_fd);
	if (!fd) return Errno(EBADF);

	Socket_fs::Context *context = dynamic_cast<Socket_fs::Context *>(fd->context);
	if (!context) return Errno(ENOTSOCK);

	switch (context->proto()) {
	case Socket_fs::Context::Proto::UDP: return Errno(ENOTCONN);
	case Socket_fs::Context::Proto::TCP:
		{
			Socket_fs::Remote_functor func(*context, false);
			return read_sockaddr_in(func, (sockaddr_in *)addr, addrlen);
		}
	}

	return 0;
}


extern "C" int socket_fs_getsockname(int libc_fd, sockaddr *addr, socklen_t *addrlen)
{
	File_descriptor *fd = file_descriptor_allocator()->find_by_libc_fd(libc_fd);
	if (!fd) return Errno(EBADF);

	Socket_fs::Context *context = dynamic_cast<Socket_fs::Context *>(fd->context);
	if (!context) return Errno(ENOTSOCK);

	Socket_fs::Local_functor func(*context, false);
	return read_sockaddr_in(func, (sockaddr_in *)addr, addrlen);
}


/**************************
 ** Socket transport API **
 **************************/

extern "C" int socket_fs_accept(int libc_fd, sockaddr *addr, socklen_t *addrlen)
{
	File_descriptor *fd = file_descriptor_allocator()->find_by_libc_fd(libc_fd);
	if (!fd) return Errno(EBADF);

	Socket_fs::Context *listen_context = dynamic_cast<Socket_fs::Context *>(fd->context);
	if (!listen_context) return Errno(ENOTSOCK);

	/* TODO EOPNOTSUPP - no SOCK_STREAM */
	/* TODO ECONNABORTED */

	char accept_buf[MAX_CONTROL_PATH_LEN];
	{
		int n = 0;
		/* XXX currently reading accept may return without new connection */
		do {
			n = read(listen_context->accept_fd(), accept_buf, sizeof(accept_buf));
		} while (n == 0);
		if (n == -1 && errno == EAGAIN)
			return Errno(EAGAIN);
		if (n == -1)
			return Errno(EINVAL);
	}

	Socket_fs::Absolute_path path { listen_context->path() };
	path.append("/accept_socket");

	int handle_fd = ::open(path.base(), O_RDONLY);
	if (handle_fd < 0) {
		error("failed to open accept socket at ", path);
		return Errno(EACCES);
	}

	Socket_fs::Context *accept_context;
	try {
		Libc::Allocator alloc { };
		accept_context = new (alloc)
			Socket_fs::Context(listen_context->proto(), handle_fd);
	} catch (New_socket_failed) {
		close(handle_fd);
		return Errno(ENFILE);
	}

	File_descriptor *accept_fd =
		file_descriptor_allocator()->alloc(&plugin(), accept_context);
	if (!accept_fd) {
		Libc::Allocator alloc { };
		destroy(alloc, accept_context);
		return Errno(EMFILE);
	}

	if (addr && addrlen) {
		Socket_fs::Remote_functor func(*accept_context, false);
		int ret = read_sockaddr_in(func, (sockaddr_in *)addr, addrlen);
		if (ret == -1) {
			file_descriptor_allocator()->free(accept_fd);
			Libc::Allocator alloc { };
			destroy(alloc, accept_context);
			return ret;
		}
	}

	/* inherit the O_NONBLOCK flag if set */
	accept_context->fd_flags(listen_context->fd_flags());

	return accept_fd->libc_fd;
}


extern "C" int socket_fs_bind(int libc_fd, sockaddr const *addr, socklen_t addrlen)
{
	File_descriptor *fd = file_descriptor_allocator()->find_by_libc_fd(libc_fd);
	if (!fd) return Errno(EBADF);

	Socket_fs::Context *context = dynamic_cast<Socket_fs::Context *>(fd->context);
	if (!context) return Errno(ENOTSOCK);

	if (!addr) return Errno(EFAULT);

	if (addr->sa_family != AF_INET) {
		error(__func__, ": family ", addr->sa_family, " not supported");
		return Errno(EAFNOSUPPORT);
	}

	Sockaddr_string addr_string;

	try {
		addr_string = Sockaddr_string(host_string(*(sockaddr_in *)addr),
		                              port_string(*(sockaddr_in *)addr));
	}
	catch (Address_conversion_failed) { return Errno(EINVAL); }

	try {
		int const len = ::strlen(addr_string.base());
		int const n   = write(context->bind_fd(), addr_string.base(), len);
		if (n != len) return Errno(EACCES);

		/* sync to block for write completion */
		return fsync(context->bind_fd());
	} catch (Socket_fs::Context::Inaccessible) {
		return Errno(EINVAL);
	}
}


extern "C" int socket_fs_connect(int libc_fd, sockaddr const *addr, socklen_t addrlen)
{
	File_descriptor *fd = file_descriptor_allocator()->find_by_libc_fd(libc_fd);
	if (!fd) return Errno(EBADF);

	Socket_fs::Context *context = dynamic_cast<Socket_fs::Context *>(fd->context);
	if (!context) return Errno(ENOTSOCK);

	if (!addr) return Errno(EFAULT);

	switch (addr->sa_family) {
	case AF_UNSPEC:
		if (context->state() != Context::CONNECTED)
			return 0;
		context->state(Context::UNSPEC); /* reset */
		break;
	case AF_INET:
		break;
	default:
		return Errno(EAFNOSUPPORT);
	}

	switch (context->state()) {
	case Context::UNSPEC:
		{
			Sockaddr_string addr_string { };
			int const len = ::strlen(addr_string.base());
			int const n   = write(context->connect_fd(), addr_string.base(), len);

			if (n != len) return Errno(ECONNREFUSED);
			context->state(Context::UNCONNECTED);

			return 0;
		}

	case Context::UNCONNECTED:
		{
			Sockaddr_string addr_string;
			try {
				addr_string = Sockaddr_string(host_string(*(sockaddr_in const *)addr),
				                              port_string(*(sockaddr_in const *)addr));
			}
			catch (Address_conversion_failed) { return Errno(EINVAL); }

			context->state(Context::CONNECTING);

			int const len = ::strlen(addr_string.base());
			int const n   = write(context->connect_fd(), addr_string.base(), len);

			if (n != len) return Errno(ECONNREFUSED);

			if (context->fd_flags() & O_NONBLOCK)
				return Errno(EINPROGRESS);

			/* block until socket is ready for writing */

			fd_set writefds;
			FD_ZERO(&writefds);
			FD_SET(libc_fd, &writefds);

			enum { CONNECT_TIMEOUT_S = 10 };
			struct timeval timeout {CONNECT_TIMEOUT_S, 0};
			int res = select(libc_fd + 1, NULL, &writefds, NULL, &timeout);

			if (res < 0) {
				/* errno has been set by select() */
				return res;
			}

			if (res == 0) {
				context->state(Context::CONNECT_ABORTED);
				return Errno(ETIMEDOUT);
			}

			int connect_status = context->read_connect_status();

			if (connect_status == 0)
				context->state(Context::CONNECTED);
			else
				context->state(Context::CONNECT_ABORTED);

			/* errno has been set by context->read_connect_status() */
			return connect_status;
		}
		break;
	case Context::ACCEPT_ONLY:
		return Errno(EINVAL);
	case Context::CONNECTING:
		{
			if (!context->connect_read_ready())
				return Errno(EALREADY);

			int connect_status = context->read_connect_status();

			if (connect_status == 0)
				context->state(Context::CONNECTED);
			else
				context->state(Context::CONNECT_ABORTED);

			/* errno was set by context->read_connect_status() */
			return connect_status;
		}
	case Context::CONNECTED:
		return Errno(EISCONN);
	case Context::CONNECT_ABORTED:
		return Errno(ECONNABORTED);
	}

	return Errno(ECONNREFUSED);
}


extern "C" int socket_fs_listen(int libc_fd, int backlog)
{
	File_descriptor *fd = file_descriptor_allocator()->find_by_libc_fd(libc_fd);
	if (!fd) return Errno(EBADF);

	Socket_fs::Context *context = dynamic_cast<Socket_fs::Context *>(fd->context);
	if (!context) return Errno(ENOTSOCK);

	char buf[MAX_CONTROL_PATH_LEN];
	int const len = ::snprintf(buf, sizeof(buf), "%d", backlog);
	int const n   = write(context->listen_fd(), buf, len);
	if (n != len) return Errno(EOPNOTSUPP);

	/* sync to block for write completion */
	int const res = fsync(context->listen_fd());
	if (res != 0) return res;

	context->state(Context::ACCEPT_ONLY);
	return 0;
}


static ssize_t do_recvfrom(File_descriptor *fd,
                           void *buf, ::size_t const len, int const flags,
                           struct sockaddr *src_addr, socklen_t *src_addrlen)
{
	Socket_fs::Context *context = dynamic_cast<Socket_fs::Context *>(fd->context);
	if (!context) return Errno(ENOTSOCK);
	if (!buf)     return Errno(EFAULT);
	if (!len)     return Errno(EINVAL);

	if (src_addr) {
		Socket_fs::Remote_functor func(*context, context->fd_flags() & O_NONBLOCK);
		int const res = read_sockaddr_in(func, (sockaddr_in *)src_addr, src_addrlen);
		if (res < 0) return res;
	}

	/* TODO ENOTCONN */
	/* TODO ECONNREFUSED */

	int data_fd = flags & MSG_PEEK ? context->peek_fd() : context->data_fd();

	try {
		if (lseek(data_fd, 0, SEEK_SET) != 0)
			return Errno(EINVAL);

		size_t out_sum = 0;

		do {
			size_t const result = read(data_fd,
			                           (char *)buf + out_sum,
			                           len - out_sum);
			if (result <= 0) { /* eof & error */
				if (out_sum)
					return out_sum;

				return result;
			}

			out_sum += result;
		} while ((flags & MSG_WAITALL) &&
		         (out_sum < len));

		return out_sum;
	} catch (Socket_fs::Context::Inaccessible) {
		return Errno(EINVAL);
	}
}


extern "C" ssize_t socket_fs_recvfrom(int libc_fd, void *buf, ::size_t len, int flags,
                                      sockaddr *src_addr, socklen_t *src_addrlen)
{
	File_descriptor *fd = file_descriptor_allocator()->find_by_libc_fd(libc_fd);
	if (!fd) return Errno(EBADF);

	return do_recvfrom(fd, buf, len, flags, src_addr, src_addrlen);
}


extern "C" ssize_t socket_fs_recv(int libc_fd, void *buf, ::size_t len, int flags)
{
	/* identical to recvfrom() with a NULL src_addr argument */
	return socket_fs_recvfrom(libc_fd, buf, len, flags, nullptr, nullptr);
}


extern "C" ssize_t socket_fs_recvmsg(int libc_fd, msghdr *msg, int flags)
{
	/* TODO just a simple implementation that handles the easy cases */
	size_t numberOfBytes = 0;
	char *data = nullptr;
	size_t length = 0;
	char *buffer;
	ssize_t res;
	size_t amount;
	socklen_t client_address_len;

	/* iterate over all msg_iov to get the number of bytes that have to be read. */
	for (int i = 0; i < msg->msg_iovlen; i++) {
		numberOfBytes += msg->msg_iov[i].iov_len;
		/*
		 * As an optimization, we set the initial values of DATA and LEN from
		 * the first non-empty iovec. This kicks-in in the case where the whole
		 * packet fits into the first iovec buffer.
		 */
		if (data == nullptr && msg->msg_iov[i].iov_len > 0) {
			data = (char*)msg->msg_iov[i].iov_base;
			length = msg->msg_iov[i].iov_len;
		}
	}

	buffer = data;

	struct sockaddr_in client_address;
	client_address_len = sizeof (client_address);

	/* do socket communication */
	res = socket_fs_recvfrom(libc_fd, buffer, length, flags,
	                         (struct sockaddr *) &client_address,
	                         &client_address_len);

	if(res < 0) {
		return res;
	}

	/* copy client address to msg_name */
	if (msg->msg_name != nullptr && client_address_len > 0) {
		if (msg->msg_namelen > client_address_len) {
			msg->msg_namelen = client_address_len;
		}

		::memcpy (msg->msg_name, &client_address, msg->msg_namelen);
	} else if (msg->msg_name != nullptr) {
		msg->msg_namelen = 0;
	}

	/* handle payload */
	if (buffer == data) {
		buffer += length;
	} else {
		amount = length;
		buffer = data;
		for (int i = 0; i < msg->msg_iovlen; i++) {
#define min(a, b)        ((a) > (b) ? (b) : (a))
			size_t copy = min (msg->msg_iov[i].iov_len, amount);
			::memcpy (msg->msg_iov[i].iov_base, buffer, copy);
			buffer += copy;
			amount -= copy;
			if (length == 0)
				break;
		}

		Libc::Allocator alloc { };
		destroy(alloc, data);
	}

	/* handle control data, not supported yet */
	msg->msg_controllen = 0;

	return res;
}


static ssize_t do_sendto(File_descriptor *fd,
                         void const *buf, ::size_t len, int flags,
                         sockaddr const *dest_addr, socklen_t dest_addrlen)
{
	Socket_fs::Context *context = dynamic_cast<Socket_fs::Context *>(fd->context);
	if (!context) return Errno(ENOTSOCK);
	if (!buf)     return Errno(EFAULT);
	if (!len)     return Errno(EINVAL);

	/* TODO ENOTCONN, EISCONN, EDESTADDRREQ */

	try {
		if (dest_addr && context->proto() == Context::Proto::UDP) {
			try {
				Sockaddr_string addr_string(host_string(*(sockaddr_in const *)dest_addr),
				                            port_string(*(sockaddr_in const *)dest_addr));

				int const len = ::strlen(addr_string.base());
				int const n   = write(context->remote_fd(), addr_string.base(), len);
				if (n != len) return Errno(EIO);
			}
			catch (Address_conversion_failed) { return Errno(EINVAL); }
		}

		lseek(context->data_fd(), 0, 0);
		ssize_t out_len = write(context->data_fd(), buf, len);

		switch (context->proto()) {
		case Socket_fs::Context::Proto::UDP:
			if (out_len == 0) return Errno(ENETDOWN);
			break;

		case Socket_fs::Context::Proto::TCP:

			/*
			 * Non-blocking write stalled
			 */
			if ((out_len == -1) && (errno == EAGAIN))
				return Errno(EAGAIN);

			/*
			 * Write errors to TCP-data files are reflected as EPIPE, which
			 * means the connection-mode socket is no longer connected. This
			 * explicitly does not differentiate ECONNRESET, which means the
			 * peer closed the connection while there was still unhandled data
			 * in the socket buffer on the remote side and sent an RST packet.
			 *
			 * TODO If the MSG_NOSIGNAL flag is not set, the SIGPIPE signal is
			 * generated to the calling thread.
			 */
			if (out_len == -1)
				return Errno(EPIPE);
			break;
		}
		return out_len;
	} catch (Socket_fs::Context::Inaccessible) {
		return Errno(EINVAL);
	}
}


extern "C" ssize_t socket_fs_sendto(int libc_fd, void const *buf, ::size_t len, int flags,
                                    sockaddr const *dest_addr, socklen_t dest_addrlen)
{
	File_descriptor *fd = file_descriptor_allocator()->find_by_libc_fd(libc_fd);
	if (!fd) return Errno(EBADF);

	return do_sendto(fd, buf, len, flags, dest_addr, dest_addrlen);
}


extern "C" ssize_t socket_fs_send(int libc_fd, void const *buf, ::size_t len, int flags)
{
	/* identical to sendto() with a NULL dest_addr argument */
	return socket_fs_sendto(libc_fd, buf, len, flags, nullptr, 0);
}


extern "C" int socket_fs_getsockopt(int libc_fd, int level, int optname,
                                    void *optval, socklen_t *optlen)
{
	File_descriptor *fd = file_descriptor_allocator()->find_by_libc_fd(libc_fd);
	if (!fd) return Errno(EBADF);

	Socket_fs::Context *context = dynamic_cast<Socket_fs::Context *>(fd->context);
	if (!context) return Errno(ENOTSOCK);

	if (!optval) return Errno(EFAULT);

	switch (level) {
	case SOL_SOCKET:
		switch (optname) {
		case SO_REUSEADDR:
			/* not yet implemented - but return true */
			*(int *)optval = 1;
			return 0;
		case SO_ERROR:
			if (context->state() == Context::CONNECTING) {

				int connect_status = context->read_connect_status();

				if (connect_status == 0) {
					*(int*)optval = 0;
					context->state(Context::CONNECTED);
				} else {
					*(int*)optval = errno;
					context->state(Context::CONNECT_ABORTED);
				}

				return 0;
			}

			/* not yet implemented - but return true */
			*(int *)optval = 0;
			return 0;
		case SO_TYPE:
			switch (context->proto()) {
			case Socket_fs::Context::Proto::UDP: *(int *)optval = SOCK_DGRAM;  break;
			case Socket_fs::Context::Proto::TCP: *(int *)optval = SOCK_STREAM; break;
			}
			return 0;
		default: return Errno(ENOPROTOOPT);
		}

	default: return Errno(EINVAL);
	}
}


extern "C" int socket_fs_setsockopt(int libc_fd, int level, int optname,
                                    void const *optval, socklen_t optlen)
{
	File_descriptor *fd = file_descriptor_allocator()->find_by_libc_fd(libc_fd);
	if (!fd) return Errno(EBADF);

	Socket_fs::Context *context = dynamic_cast<Socket_fs::Context *>(fd->context);
	if (!context) return Errno(ENOTSOCK);

	if (!optval) return Errno(EFAULT);

	switch (level) {
	case SOL_SOCKET:
		switch (optname) {
		case SO_REUSEADDR:
			/* not yet implemented - always return true */
			return 0;
		case SO_LINGER:
			{
				linger *l = (linger *)optval;
				if (l->l_onoff == 0)
					return 0;
			}
		default: return Errno(ENOPROTOOPT);
		}
	case IPPROTO_TCP:
		switch (optname) {
			case TCP_NODELAY:
				return 0;
			default: return Errno(ENOPROTOOPT);
		}

	default: return Errno(EINVAL);
	}
}


extern "C" int socket_fs_shutdown(int libc_fd, int how)
{
	File_descriptor *fd = file_descriptor_allocator()->find_by_libc_fd(libc_fd);
	if (!fd) return Errno(EBADF);

	Socket_fs::Context *context = dynamic_cast<Socket_fs::Context *>(fd->context);
	if (!context) return Errno(ENOTSOCK);

	/* TODO ENOTCONN */
	/* TODO EINVAL - returned if 'how' is not supported but we don't support
	   shutdown at all currently */

	return 0;
}


extern "C" int socket_fs_socket(int domain, int type, int protocol)
{
	Socket_fs::Absolute_path path(config_socket());

	if (path == "") {
		error(__func__, ": socket fs not mounted");
		return Errno(EACCES);
	}
	/*
	 * The socket type (in the lower bits) maybe ORed with SOCK_CLOEXEC and
	 * SOCK_NONBLOCK options (in the higher bits). Currently, supported values
	 * are SOCK_STREAM (1) and SOCK_DGRAM (2), so just take the lower 2 bits.
	 */
	int const sock_type = type & 3;
	if ((sock_type != SOCK_STREAM || (protocol != 0 && protocol != IPPROTO_TCP))
	 && (sock_type != SOCK_DGRAM  || (protocol != 0 && protocol != IPPROTO_UDP))) {
		error(__func__,
		      ": socket with type=", (Hex)type,
		      " protocol=", (Hex)protocol, " not supported");
		return Errno(EAFNOSUPPORT);
	}

	/* socket is ensured to be TCP or UDP */
	typedef Socket_fs::Context::Proto Proto;
	Proto proto = (sock_type == SOCK_STREAM) ? Proto::TCP : Proto::UDP;
	Socket_fs::Context *context = nullptr;
	try {
		switch (proto) {
		case Proto::TCP: path.append("/tcp"); break;
		case Proto::UDP: path.append("/udp"); break;
		}

		path.append("/new_socket");
		int handle_fd = ::open(path.base(), O_RDONLY);
		if (handle_fd < 0) {
			error("failed to open new socket at ", path);
			return Errno(EACCES);
		}
		Libc::Allocator alloc { };
		context = new (alloc)
			Socket_fs::Context(proto, handle_fd);
	} catch (New_socket_failed) { return Errno(ENFILE); }

	if (context) {
		int flags = 0;
		if (type & SOCK_NONBLOCK) flags |= O_NONBLOCK;
		if (type & SOCK_CLOEXEC)  flags |= O_CLOEXEC;
		context->fd_flags(flags);
	}

	File_descriptor *fd = file_descriptor_allocator()->alloc(&plugin(), context);
	if (!fd) {
		Libc::Allocator alloc { };
		destroy(alloc, context);
		return Errno(EMFILE);
	}

	return fd->libc_fd;
}


static int read_ifaddr_file(sockaddr_in &sockaddr, Socket_fs::Absolute_path const &path)
{
	Host_string address;
	Port_string service;
	*service.base() = '0';

	{
		FILE *fp = ::fopen(path.base(), "r");
		if (!fp) return -1;

		::fscanf(fp, "%s\n", address.base());
		::fclose(fp);
	}

	try { sockaddr = sockaddr_in_struct(address, service); }
	catch (...) { return -1; }

	return 0;
}


extern "C" int getifaddrs(struct ifaddrs **ifap)
{
	static Pthread_mutex mutex;

	Pthread_mutex::Guard guard(mutex);

	static sockaddr_in address;
	static sockaddr_in netmask   { 0 };
	static sockaddr_in broadcast { 0 };
	static char        name[1] { };

	static ifaddrs ifaddr {
		.ifa_name      = name,
		.ifa_flags     = IFF_UP,
		.ifa_addr      = (sockaddr*)&address,
		.ifa_netmask   = (sockaddr*)&netmask,
		.ifa_broadaddr = (sockaddr*)&broadcast,
	};

	*ifap = &ifaddr;

	using Socket_fs::Absolute_path;

	Absolute_path const root(config_socket());

	if (read_ifaddr_file(address, Absolute_path("address", root.base())))
		return -1;

	read_ifaddr_file(netmask, Absolute_path("netmask", root.base()));
	return 0;
}


extern "C" void freeifaddrs(struct ifaddrs *) { }


/****************************
 ** File-plugin operations **
 ****************************/

int Socket_fs::Plugin::fcntl(File_descriptor *fd, int cmd, long arg)
{
	Socket_fs::Context *context = dynamic_cast<Socket_fs::Context *>(fd->context);
	if (!context) return Errno(EBADF);

	switch (cmd) {
	case F_GETFD:
		return context->fd_flags();
	case F_SETFD:
		context->fd_flags(arg);
		return 0;
	case F_GETFL:
		return context->fd_flags() | O_RDWR;
	case F_SETFL:
		context->fd_flags(arg);
		return 0;
	default:
		error(__func__, " command ", cmd, " not supported on sockets");
		return Errno(EINVAL);
	}
}

ssize_t Socket_fs::Plugin::read(File_descriptor *fd, void *buf, ::size_t count)
{
	ssize_t const ret = do_recvfrom(fd, buf, count, 0, nullptr, nullptr);
	if (ret != -1) return ret;

	/* TODO map recvfrom errno to write errno */
	switch (errno) {
	default: return Errno(errno);
	}
}


ssize_t Socket_fs::Plugin::write(File_descriptor *fd, const void *buf, ::size_t count)
{

	ssize_t const ret = do_sendto(fd, buf, count, 0, nullptr, 0);
	if (ret != -1) return ret;

	/* TODO map sendto errno to write errno */
	switch (errno) {
	default: return Errno(errno);
	}
}


bool Socket_fs::Plugin::poll(File_descriptor &fdo, struct pollfd &pfd)
{
	if (fdo.plugin != this) return false;
	Socket_fs::Context *context { nullptr };

	try {
		context = dynamic_cast<Socket_fs::Context *>(fdo.context);
	} catch (Socket_fs::Context::Inaccessible) {
		pfd.revents |= POLLNVAL;
		return true;
	}

	enum {
		POLLIN_MASK = POLLIN | POLLRDNORM | POLLRDBAND | POLLPRI,
		POLLOUT_MASK = POLLOUT | POLLWRNORM | POLLWRBAND,
	};

	bool res { false };

	if ((pfd.events & POLLIN_MASK) && context->read_ready()) {
		pfd.revents |= pfd.events & POLLIN_MASK;
		res = true;
	}

	if ((pfd.events & POLLOUT_MASK) && context->write_ready()) {
		pfd.revents |= pfd.events & POLLOUT_MASK;
		res = true;
	}

	return res;
}


bool Socket_fs::Plugin::supports_select(int nfds,
                                        fd_set *readfds, fd_set *writefds, fd_set *exceptfds,
                                        struct timeval *timeout)
{
	/* return true if any file descriptor (which is set) belongs to the VFS */
	for (int fd = 0; fd < nfds; ++fd) {

		if (FD_ISSET(fd, readfds) || FD_ISSET(fd, writefds) || FD_ISSET(fd, exceptfds)) {
			File_descriptor *fdo = file_descriptor_allocator()->find_by_libc_fd(fd);

			if (fdo && (fdo->plugin == this))
				return true;
		}
	}

	return false;
}


int Socket_fs::Plugin::select(int nfds,
                              fd_set *readfds, fd_set *writefds, fd_set *exceptfds,
                              timeval *timeout)
{
	int nready = 0;

	fd_set const in_readfds  = *readfds;
	fd_set const in_writefds = *writefds;
	/* XXX exceptfds not supported */

	/* clear fd sets */
	FD_ZERO(readfds);
	FD_ZERO(writefds);
	FD_ZERO(exceptfds);

	auto fn = [&] {

		for (int fd = 0; fd < nfds; ++fd) {

			bool fd_in_readfds = FD_ISSET(fd, &in_readfds);
			bool fd_in_writefds = FD_ISSET(fd, &in_writefds);

			if (!fd_in_readfds && !fd_in_writefds)
				continue;

			File_descriptor *fdo = file_descriptor_allocator()->find_by_libc_fd(fd);

			/* handle only fds that belong to this plugin */
			if (!fdo || (fdo->plugin != this))
				continue;

			if (fd_in_readfds) {
				try {
					Socket_fs::Context *context = dynamic_cast<Socket_fs::Context *>(fdo->context);

					if (context->read_ready()) {
						FD_SET(fd, readfds);
						++nready;
					}
				} catch (Socket_fs::Context::Inaccessible) { }
			}

			if (fd_in_writefds) {
				try {
					Socket_fs::Context *context = dynamic_cast<Socket_fs::Context *>(fdo->context);
					if (context->write_ready()) {
						FD_SET(fd, writefds);
						++nready;
					}
				} catch (Socket_fs::Context::Inaccessible) { }
			}

			/* XXX exceptfds not supported */
		}
		return Fn::COMPLETE;
	};

	if (Libc::Kernel::kernel().main_context() && Libc::Kernel::kernel().main_suspended()) {
		fn();
	} else {
		monitor().monitor(fn);
	}

	return nready;
}


int Socket_fs::Plugin::close(File_descriptor *fd)
{
	Socket_fs::Context *context = dynamic_cast<Socket_fs::Context *>(fd->context);
	if (!context) return Errno(EBADF);

	Libc::Allocator alloc { };
	destroy(alloc, context);
	file_descriptor_allocator()->free(fd);

	/*
	 * the socket is freed when the initial handle
	 * on 'new_socket' is released at the VFS plugin
	 */

	return 0;
}


int Socket_fs::Plugin::ioctl(File_descriptor *, unsigned long request, char*)
{
	if (request == FIONREAD) {
		/*
		 * This request occurs quite often when using the Arora web browser,
		 * so print the error message only once.
		 */
		static bool print_fionread_error_message = true;
		if (print_fionread_error_message) {
			error(__func__, " request FIONREAD not supported on sockets"
			      " (this message will not be shown again)");
			print_fionread_error_message = false;
		}
		errno = EINVAL;
		return -1;
	}

	error(__func__, " request ", request, " not supported on sockets");
	errno = ENOTTY;
	return -1;
}


Libc::Socket_fs::Plugin &Libc::Socket_fs::plugin()
{
	static Socket_fs::Plugin inst;
	return inst;
}
