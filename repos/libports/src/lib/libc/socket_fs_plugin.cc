/*
 * \brief  Libc pseudo plugin for socket fs
 * \author Christian Helmuth
 * \author Christian Prochaska
 * \author Norman Feske
 * \author Emery Hemingway
 * \date   2015-06-23
 */

/*
 * Copyright (C) 2015-2017 Genode Labs GmbH
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
#include <stdio.h>

/* libc-internal includes */
#include "socket_fs_plugin.h"
#include "libc_file.h"
#include "libc_errno.h"
#include "task.h"


namespace Libc {
	extern char const *config_socket();
	bool read_ready(Libc::File_descriptor *);
}


/***************
 ** Utilities **
 ***************/

namespace Socket_fs {

	Libc::Allocator global_allocator;

	using Libc::Errno;

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

	struct Exception { };
	struct New_socket_failed : Exception { };
	struct Address_conversion_failed : Exception { };

	struct Context;
	struct Plugin;
	struct Sockaddr_functor;
	struct Remote_functor;
	struct Local_functor;

	Plugin & plugin();

	enum { MAX_CONTROL_PATH_LEN = 16 };
}


using namespace Socket_fs;


struct Socket_fs::Context : Libc::Plugin_context
{
	private:

		int const _handle_fd;

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

	public:

		enum Proto { TCP, UDP };

		struct Inaccessible { }; /* exception */

		Absolute_path const path {
			_read_socket_path().base(), Libc::config_socket() };

	private:

		enum Fd : unsigned {
			DATA, CONNECT, BIND, LISTEN, ACCEPT, LOCAL, REMOTE, MAX
		};

		struct
		{
			char const            *name;
			int                    num;
			Libc::File_descriptor *file;
		} _fd[Fd::MAX] = {
			{ "data",    -1, nullptr },
			{ "connect", -1, nullptr }, { "bind",   -1, nullptr },
			{ "listen",  -1, nullptr }, { "accept", -1, nullptr },
			{ "local",   -1, nullptr }, { "remote", -1, nullptr }
		};

		int  _fd_flags    = 0;

		Proto const _proto;

		bool _accept_only = false;

		template <typename FUNC>
		void _fd_apply(FUNC const &fn)
		{
			for (unsigned i = 0; i < Fd::MAX; ++i)
				if (_fd[i].num != -1) fn(_fd[i].num);
		}

		int _fd_for_type(Fd type, int flags)
		{
			/* open file on demand */
			if (_fd[type].num == -1) {
				Absolute_path file(_fd[type].name, path.base());
				int const fd = open(file.base(), flags|_fd_flags);
				if (fd == -1) {
					Genode::error(__func__, ": ", _fd[type].name, " file not accessible at ", file);
					throw Inaccessible();
				}
				_fd[type].num  = fd;
				_fd[type].file = Libc::file_descriptor_allocator()->find_by_libc_fd(fd);
			}

			return _fd[type].num;
		}

		bool _fd_read_ready(Fd type)
		{
			if (_fd[type].file)
				return Libc::read_ready(_fd[type].file);
			else
				return false;
		}

	public:

		Context(Proto proto, int handle_fd)
		: _handle_fd(handle_fd), _proto(proto) { }

		~Context()
		{
			_fd_apply([] (int fd) { ::close(fd); });
			::close(_handle_fd);
		}

		Proto proto() const { return _proto; }

		int fd_flags() const { return _fd_flags; }
		void fd_flags(int flags)
		{
			_fd_flags = flags;
			_fd_apply([flags] (int fd) { fcntl(fd, F_SETFL, flags); });
		}

		int data_fd()    { return _fd_for_type(Fd::DATA,    O_RDWR); }
		int connect_fd() { return _fd_for_type(Fd::CONNECT, O_WRONLY); }
		int bind_fd()    { return _fd_for_type(Fd::BIND,    O_WRONLY); }
		int listen_fd()  { return _fd_for_type(Fd::LISTEN,  O_WRONLY); }
		int accept_fd()  { return _fd_for_type(Fd::ACCEPT,  O_RDONLY); }
		int local_fd()   { return _fd_for_type(Fd::LOCAL,   O_RDWR); }
		int remote_fd()  { return _fd_for_type(Fd::REMOTE,  O_RDWR); }

		/* request the appropriate fd to ensure the file is open */
		bool data_read_ready()   { data_fd();   return _fd_read_ready(Fd::DATA); }
		bool accept_read_ready() { accept_fd(); return _fd_read_ready(Fd::ACCEPT); }
		bool local_read_ready()  { local_fd();  return _fd_read_ready(Fd::LOCAL); }
		bool remote_read_ready() { remote_fd(); return _fd_read_ready(Fd::REMOTE); }

		void accept_only() { _accept_only = true; }

		bool read_ready()
		{
			return _accept_only ? accept_read_ready() : data_read_ready();
		}
};


struct Socket_fs::Sockaddr_functor : Libc::Suspend_functor
{
	Socket_fs::Context &context;
	bool const          nonblocking;

	Sockaddr_functor(Socket_fs::Context &context, bool nonblocking)
	: context(context), nonblocking(nonblocking) { }

	virtual int fd() = 0;
};


struct Socket_fs::Remote_functor : Socket_fs::Sockaddr_functor
{
	Remote_functor(Socket_fs::Context &context, bool nonblocking)
	: Sockaddr_functor(context, nonblocking) { }

	bool suspend() override { return !nonblocking && !context.remote_read_ready(); }
	int fd()       override { return context.remote_fd(); }
};


struct Socket_fs::Local_functor : Socket_fs::Sockaddr_functor
{
	Local_functor(Socket_fs::Context &context, bool nonblocking)
	: Sockaddr_functor(context, nonblocking) { }

	bool suspend() override { return !nonblocking && !context.local_read_ready(); }
	int fd()       override { return context.local_fd(); }
};


struct Socket_fs::Plugin : Libc::Plugin
{
	bool supports_select(int, fd_set *, fd_set *, fd_set *, timeval *) override;

	ssize_t read(Libc::File_descriptor *, void *, ::size_t) override;
	ssize_t write(Libc::File_descriptor *, const void *, ::size_t) override;
	int fcntl(Libc::File_descriptor *, int, long) override;
	int close(Libc::File_descriptor *) override;
	int select(int, fd_set *, fd_set *, fd_set *, timeval *) override;
};


template <int CAPACITY> class Socket_fs::String
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
struct Socket_fs::Sockaddr_string : String<NI_MAXHOST + NI_MAXSERV>
{
	Sockaddr_string() { }

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

		strncpy(host.base(), base(), host.capacity());
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

		strncpy(port.base(), ++at, port.capacity());

		return port;
	}
};


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

	memset(&hints, 0, sizeof(hints));
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

	while (!func.nonblocking && func.suspend())
		Libc::suspend(func);

	Sockaddr_string addr_string;
	int const n = read(func.fd(), addr_string.base(), addr_string.capacity() - 1);

	if (n == -1) return Errno(errno);
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
		memcpy(addr, &saddr, *addrlen);
		*addrlen = sizeof(saddr);

		return 0;
	} catch (Address_conversion_failed) {
		Genode::warning("IP address conversion failed");
		return Errno(ENOBUFS);
	}
}


/***********************
 ** Address functions **
 ***********************/

extern "C" int socket_fs_getpeername(int libc_fd, sockaddr *addr, socklen_t *addrlen)
{
	Libc::File_descriptor *fd = Libc::file_descriptor_allocator()->find_by_libc_fd(libc_fd);
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
	Libc::File_descriptor *fd = Libc::file_descriptor_allocator()->find_by_libc_fd(libc_fd);
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
	Libc::File_descriptor *fd = Libc::file_descriptor_allocator()->find_by_libc_fd(libc_fd);
	if (!fd) return Errno(EBADF);

	Socket_fs::Context *listen_context = dynamic_cast<Socket_fs::Context *>(fd->context);
	if (!listen_context) return Errno(ENOTSOCK);

	/* TODO EOPNOTSUPP - no SOCK_STREAM */
	/* TODO ECONNABORTED */

	char accept_buf[8];
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

	Absolute_path path = listen_context->path;
	path.append("/accept_socket");

	int handle_fd = ::open(path.base(), O_RDONLY);
	if (handle_fd < 0) {
		Genode::error("failed to open accept socket at ", path);
		return Errno(EACCES);
	}

	Socket_fs::Context *accept_context;
	try {
		accept_context = new (&global_allocator)
			Socket_fs::Context(listen_context->proto(), handle_fd);
	} catch (New_socket_failed) { return Errno(EACCES); }

	Libc::File_descriptor *accept_fd =
		Libc::file_descriptor_allocator()->alloc(&plugin(), accept_context);

	/* inherit the O_NONBLOCK flag if set */
	accept_context->fd_flags(listen_context->fd_flags());

	if (addr && addrlen) {
		Socket_fs::Remote_functor func(*accept_context, false);
		int ret = read_sockaddr_in(func, (sockaddr_in *)addr, addrlen);
		if (ret == -1) return ret;
	}

	return accept_fd->libc_fd;
}


extern "C" int socket_fs_bind(int libc_fd, sockaddr const *addr, socklen_t addrlen)
{
	Libc::File_descriptor *fd = Libc::file_descriptor_allocator()->find_by_libc_fd(libc_fd);
	if (!fd) return Errno(EBADF);

	Socket_fs::Context *context = dynamic_cast<Socket_fs::Context *>(fd->context);
	if (!context) return Errno(ENOTSOCK);

	if (!addr) return Errno(EFAULT);

	if (addr->sa_family != AF_INET) {
		Genode::error(__func__, ": family not supported");
		return Errno(EAFNOSUPPORT);
	}

	Sockaddr_string addr_string;

	try {
		addr_string = Sockaddr_string(host_string(*(sockaddr_in *)addr),
		                              port_string(*(sockaddr_in *)addr));
	}
	catch (Address_conversion_failed) { return Errno(EINVAL); }

	try {
		int const len = strlen(addr_string.base());
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
	Libc::File_descriptor *fd = Libc::file_descriptor_allocator()->find_by_libc_fd(libc_fd);
	if (!fd) return Errno(EBADF);

	Socket_fs::Context *context = dynamic_cast<Socket_fs::Context *>(fd->context);
	if (!context) return Errno(ENOTSOCK);

	if (!addr) return Errno(EFAULT);

	if (addr->sa_family != AF_INET) {
		Genode::error(__func__, ": family not supported");
		return Errno(EAFNOSUPPORT);
	}

	/* TODO EISCONN */
	/* TODO ECONNREFUSED */
	/* TODO maybe EALREADY, EINPROGRESS, ETIMEDOUT */

	Sockaddr_string addr_string;
	try {
		addr_string = Sockaddr_string(host_string(*(sockaddr_in const *)addr),
		                              port_string(*(sockaddr_in const *)addr));
	}
	catch (Address_conversion_failed) { return Errno(EINVAL); }

	int const len = strlen(addr_string.base());
	int const n   = write(context->connect_fd(), addr_string.base(), len);
	if (n != len) return Errno(ECONNREFUSED);

	/* sync to block for write completion */
	return fsync(context->connect_fd());
}


extern "C" int socket_fs_listen(int libc_fd, int backlog)
{
	Libc::File_descriptor *fd = Libc::file_descriptor_allocator()->find_by_libc_fd(libc_fd);
	if (!fd) return Errno(EBADF);

	Socket_fs::Context *context = dynamic_cast<Socket_fs::Context *>(fd->context);
	if (!context) return Errno(ENOTSOCK);

	char buf[MAX_CONTROL_PATH_LEN];
	int const len = snprintf(buf, sizeof(buf), "%d", backlog);
	int const n   = write(context->listen_fd(), buf, len);
	if (n != len) return Errno(EOPNOTSUPP);

	context->accept_only();
	return 0;
}


static ssize_t do_recvfrom(Libc::File_descriptor *fd,
                           void *buf, ::size_t len, int flags,
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

	try {
		lseek(context->data_fd(), 0, 0);
		ssize_t out_len = read(context->data_fd(), buf, len);
		return out_len;
	} catch (Socket_fs::Context::Inaccessible) {
		return Errno(EINVAL);
	}
}


extern "C" ssize_t socket_fs_recvfrom(int libc_fd, void *buf, ::size_t len, int flags,
                                      sockaddr *src_addr, socklen_t *src_addrlen)
{
	Libc::File_descriptor *fd = Libc::file_descriptor_allocator()->find_by_libc_fd(libc_fd);
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
	Genode::warning("##########  TODO  ########## ", __func__);
	return 0;
}


static ssize_t do_sendto(Libc::File_descriptor *fd,
                         void const *buf, ::size_t len, int flags,
                         sockaddr const *dest_addr, socklen_t dest_addrlen)
{
	Socket_fs::Context *context = dynamic_cast<Socket_fs::Context *>(fd->context);
	if (!context) return Errno(ENOTSOCK);
	if (!buf)     return Errno(EFAULT);
	if (!len)     return Errno(EINVAL);

	/* TODO ENOTCONN, EISCONN, EDESTADDRREQ */
	/* TODO ECONNRESET */

	try {
		if (dest_addr && context->proto() == Context::Proto::UDP) {
			try {
				Sockaddr_string addr_string(host_string(*(sockaddr_in const *)dest_addr),
				                            port_string(*(sockaddr_in const *)dest_addr));

				int const len = strlen(addr_string.base());
				int const n   = write(context->remote_fd(), addr_string.base(), len);
				if (n != len) return Errno(EIO);
			}
			catch (Address_conversion_failed) { return Errno(EINVAL); }
		}

		lseek(context->data_fd(), 0, 0);
		ssize_t out_len = write(context->data_fd(), buf, len);
		if (out_len == 0) {
			switch (context->proto()) {
				case Socket_fs::Context::Proto::UDP: return Errno(ENETDOWN);
				case Socket_fs::Context::Proto::TCP: return Errno(EAGAIN);
			}
		}
		return out_len;
	} catch (Socket_fs::Context::Inaccessible) {
		return Errno(EINVAL);
	}
}


extern "C" ssize_t socket_fs_sendto(int libc_fd, void const *buf, ::size_t len, int flags,
                                    sockaddr const *dest_addr, socklen_t dest_addrlen)
{
	Libc::File_descriptor *fd = Libc::file_descriptor_allocator()->find_by_libc_fd(libc_fd);
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
	Libc::File_descriptor *fd = Libc::file_descriptor_allocator()->find_by_libc_fd(libc_fd);
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
	Libc::File_descriptor *fd = Libc::file_descriptor_allocator()->find_by_libc_fd(libc_fd);
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
		default: return Errno(ENOPROTOOPT);
		}

	default: return Errno(EINVAL);
	}
}


extern "C" int socket_fs_shutdown(int libc_fd, int how)
{
	Libc::File_descriptor *fd = Libc::file_descriptor_allocator()->find_by_libc_fd(libc_fd);
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
	Absolute_path path(Libc::config_socket());

	if (path == "") {
		Genode::error(__func__, ": socket fs not mounted");
		return Errno(EACCES);
	}

	if ((type != SOCK_STREAM || (protocol != 0 && protocol != IPPROTO_TCP))
	 && (type != SOCK_DGRAM  || (protocol != 0 && protocol != IPPROTO_UDP))) {
		Genode::error(__func__,
		              ": socket with type=", type,
		              " protocol=", protocol, " not supported");
		return Errno(EAFNOSUPPORT);
	}

	/* socket is ensured to be TCP or UDP */
	typedef Socket_fs::Context::Proto Proto;
	Proto proto = (type == SOCK_STREAM) ? Proto::TCP : Proto::UDP;
	Socket_fs::Context *context = nullptr;
	try {
		switch (proto) {
		case Proto::TCP: path.append("/tcp"); break;
		case Proto::UDP: path.append("/udp"); break;
		}

		path.append("/new_socket");
		int handle_fd = ::open(path.base(), O_RDONLY);
		if (handle_fd < 0) {
			Genode::error("failed to open new socket at ", path);
			return Errno(EACCES);
		}
		context = new (&global_allocator)
			Socket_fs::Context(proto, handle_fd);
	} catch (New_socket_failed) { return Errno(EACCES); }

	Libc::File_descriptor *fd =
		Libc::file_descriptor_allocator()->alloc(&plugin(), context);

	return fd->libc_fd;
}


/****************************
 ** File-plugin operations **
 ****************************/

int Socket_fs::Plugin::fcntl(Libc::File_descriptor *fd, int cmd, long arg)
{
	Socket_fs::Context *context = dynamic_cast<Socket_fs::Context *>(fd->context);
	if (!context) return Errno(EBADF);

	switch (cmd) {
	case F_GETFL:
		return context->fd_flags();
	case F_SETFL:
		context->fd_flags(arg);
		return 0;
	default:
		Genode::error(__func__, " command ", cmd, " not supported on sockets");
		return Errno(EINVAL);
	}
}

ssize_t Socket_fs::Plugin::read(Libc::File_descriptor *fd, void *buf, ::size_t count)
{
	ssize_t const ret = do_recvfrom(fd, buf, count, 0, nullptr, nullptr);
	if (ret != -1) return ret;

	/* TODO map recvfrom errno to write errno */
	switch (errno) {
	default: return Errno(errno);
	}
}


ssize_t Socket_fs::Plugin::write(Libc::File_descriptor *fd, const void *buf, ::size_t count)
{

	ssize_t const ret = do_sendto(fd, buf, count, 0, nullptr, 0);
	if (ret != -1) return ret;

	/* TODO map sendto errno to write errno */
	switch (errno) {
	default: return Errno(errno);
	}
}


bool Socket_fs::Plugin::supports_select(int nfds,
                                        fd_set *readfds, fd_set *writefds, fd_set *exceptfds,
                                        struct timeval *timeout)
{
	/* return true if any file descriptor (which is set) belongs to the VFS */
	for (int fd = 0; fd < nfds; ++fd) {

		if (FD_ISSET(fd, readfds) || FD_ISSET(fd, writefds) || FD_ISSET(fd, exceptfds)) {
			Libc::File_descriptor *fdo =
				Libc::file_descriptor_allocator()->find_by_libc_fd(fd);

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

	for (int fd = 0; fd < nfds; ++fd) {

		Libc::File_descriptor *fdo =
			Libc::file_descriptor_allocator()->find_by_libc_fd(fd);

		/* handle only fds that belong to this plugin */
		if (!fdo || (fdo->plugin != this))
			continue;

		if (FD_ISSET(fd, &in_readfds)) {
			try {
				Socket_fs::Context *context = dynamic_cast<Socket_fs::Context *>(fdo->context);

				if (context->read_ready()) {
					FD_SET(fd, readfds);
					++nready;
				}
			} catch (Socket_fs::Context::Inaccessible) { }
		}

		if (FD_ISSET(fd, &in_writefds)) {
			if (true /* XXX ask if "data" is writeable */) {
				FD_SET(fd, writefds);
				++nready;
			}
		}

		/* XXX exceptfds not supported */
	}

	return nready;
}


int Socket_fs::Plugin::close(Libc::File_descriptor *fd)
{
	Socket_fs::Context *context = dynamic_cast<Socket_fs::Context *>(fd->context);
	if (!context) return Errno(EBADF);

	Genode::destroy(&global_allocator, context);
	Libc::file_descriptor_allocator()->free(fd);

	/*
	 * the socket is freed when the initial handle
	 * on 'new_socket' is released at the VFS plugin
	 */

	return 0;
}


Plugin & Socket_fs::plugin()
{
	static Plugin inst;
	return inst;
}
