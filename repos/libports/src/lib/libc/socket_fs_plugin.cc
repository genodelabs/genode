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

namespace {

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

	struct Exception { };
	struct New_socket_failed : Exception { };
	struct Address_conversion_failed : Exception { };

	struct Socket_context : Libc::Plugin_context
	{
		struct Inaccessible { }; /* exception */

		Absolute_path path;

		int _data_fd   = -1;
		int _remote_fd = -1;
		int _local_fd  = -1;

		Socket_context(Absolute_path const &path)
		: path(path.base()) { }

		~Socket_context()
		{
			if (_data_fd != -1)   close(_data_fd);
			if (_local_fd != -1)  close(_local_fd);
			if (_remote_fd != -1) close(_remote_fd);
		}

		int _open_file(char const *file_name)
		{
			Absolute_path file(file_name, path.base());
			int const fd = open(file.base(), O_RDWR);
			if (fd == -1) {
				Genode::error(__func__, ": ", file_name, " file not accessible");
				throw Inaccessible();
			}
			return fd;
		}

		int data_fd()
		{
			if (_data_fd == -1)
				_data_fd = _open_file("data");

			return _data_fd;
		}

		bool data_read_ready()
		{
			return Libc::read_ready(
				Libc::file_descriptor_allocator()->find_by_libc_fd(data_fd()));
		}

		int local_fd()
		{
			if (_local_fd == -1)
				_local_fd = _open_file("local");

			return _local_fd;
		}

		int remote_fd()
		{
			if (_remote_fd == -1)
				_remote_fd = _open_file("remote");

			return _remote_fd;
		}

		bool remote_read_ready()
		{
			return Libc::read_ready(
				Libc::file_descriptor_allocator()->find_by_libc_fd(remote_fd()));
		}
	};

	struct Socket_plugin : Libc::Plugin
	{
		bool supports_select(int, fd_set *, fd_set *, fd_set *, timeval *) override;

		ssize_t read(Libc::File_descriptor *, void *, ::size_t) override;
		ssize_t write(Libc::File_descriptor *, const void *, ::size_t) override;
		int close(Libc::File_descriptor *) override;
		int select(int, fd_set *, fd_set *, fd_set *, timeval *) override;
	};

	Socket_plugin & socket_plugin()
	{
		static Socket_plugin inst;
		return inst;
	}

	template <int CAPACITY> class String
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

	typedef String<NI_MAXHOST> Host_string;
	typedef String<NI_MAXSERV> Port_string;

	/*
	 * Both NI_MAXHOST and NI_MAXSERV include the terminating 0, which allows
	 * use to put ':' between host and port on concatenation.
	 */
	struct Sockaddr_string : String<NI_MAXHOST + NI_MAXSERV>
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
}


/* TODO move to C++ structs or something */

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


static int read_sockaddr_in(int read_fd, Socket_context *context,
                            struct sockaddr_in *addr, socklen_t *addrlen,
                            bool block_for_read = false)
{
	if (!addr)                     return Errno(EFAULT);
	if (!addrlen || *addrlen <= 0) return Errno(EINVAL);
	if (read_fd == -1)             return Errno(ENOTCONN);

	struct Check : Libc::Suspend_functor {
		Socket_context *context;
		Check (Socket_context *context) : context (context) { }
		bool suspend() override {
			return !context->remote_read_ready(); }
	} check ( context );

	while (block_for_read && !context->remote_read_ready())
		Libc::suspend(check);

	Sockaddr_string addr_string;
	int const n = read(read_fd, addr_string.base(), addr_string.capacity() - 1);

	if (!n) /* probably a UDP socket */
		return Errno(ENOTCONN);
	if (n == -1 || !n || n >= (int)addr_string.capacity() - 1)
		return Errno(EINVAL);

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

	Socket_context *context;
	if (!(context = dynamic_cast<Socket_context *>(fd->context)))
		return Errno(ENOTSOCK);

	return read_sockaddr_in(context->remote_fd(), context, (sockaddr_in *)addr, addrlen);
}


extern "C" int socket_fs_getsockname(int libc_fd, sockaddr *addr, socklen_t *addrlen)
{
	Libc::File_descriptor *fd = Libc::file_descriptor_allocator()->find_by_libc_fd(libc_fd);
	if (!fd) return Errno(EBADF);

	Socket_context *context;
	if (!(context = dynamic_cast<Socket_context *>(fd->context)))
		return Errno(ENOTSOCK);

	return read_sockaddr_in(context->local_fd(), context, (sockaddr_in *)addr, addrlen);
}


/**************************
 ** Socket transport API **
 **************************/

extern "C" int socket_fs_accept(int libc_fd, sockaddr *addr, socklen_t *addrlen)
{
	Libc::File_descriptor *fd = Libc::file_descriptor_allocator()->find_by_libc_fd(libc_fd);
	if (!fd)
		return Errno(EBADF);
	Socket_context *context;
	if (!(context = dynamic_cast<Socket_context *>(fd->context)))
		return Errno(ENOTSOCK);

	char accept_socket[10];
	{
		Absolute_path file("accept", context->path.base());
		int const fd = open(file.base(), O_RDONLY);
		if (fd == -1) {
			Genode::error(__func__, ": accept file not accessible");
			return Errno(EINVAL);
		}
		int n = 0;
		/* XXX currently reading accept may return without new connection */
		do { n = read(fd, accept_socket, sizeof(accept_socket)); } while (n == 0);
		close(fd);
		if (n == -1 || n >= (int)sizeof(accept_socket) - 1)
			return Errno(EINVAL);

		accept_socket[n] = 0;
	}

	Absolute_path accept_path(accept_socket, Libc::config_socket());
	Socket_context *accept_context = new (&global_allocator)
	                                      Socket_context(accept_path);
	Libc::File_descriptor *accept_fd =
		Libc::file_descriptor_allocator()->alloc(&socket_plugin(), accept_context);

	if (addr && addrlen) {
		Absolute_path file("remote", accept_path.base());
		int const fd = open(file.base(), O_RDONLY);
		if (fd == -1) {
			Genode::error(__func__, ": remote file not accessible");
			return Errno(EINVAL);
		}
		Sockaddr_string remote;
		int const n = read(fd, remote.base(), remote.capacity() - 1);
		close(fd);
		if (n == -1 || !n || n >= (int)remote.capacity() - 1)
			return Errno(EINVAL);

		remote.terminate(n);
		remote.remove_trailing_newline();

		sockaddr_in remote_addr = sockaddr_in_struct(remote.host(), remote.port());
		memcpy(addr, &remote_addr, *addrlen);
		*addrlen = sizeof(remote_addr);
	}

	return accept_fd->libc_fd;
}


extern "C" int socket_fs_bind(int libc_fd, sockaddr const *addr, socklen_t addrlen)
{
	if (addr->sa_family != AF_INET) {
		Genode::error(__func__, ": family not supported");
		return Errno(EAFNOSUPPORT);
	}

	Libc::File_descriptor *fd = Libc::file_descriptor_allocator()->find_by_libc_fd(libc_fd);
	if (!fd)
		return Errno(EBADF);
	Socket_context *context;
	if (!(context = dynamic_cast<Socket_context *>(fd->context)))
		return Errno(ENOTSOCK);

	{
		Sockaddr_string addr_string(host_string(*(sockaddr_in *)addr),
		                            port_string(*(sockaddr_in *)addr));

		Absolute_path file("bind", context->path.base());
		int const fd = open(file.base(), O_WRONLY);
		if (fd == -1) {
			Genode::error(__func__, ": bind file not accessible");
			return Errno(EINVAL);
		}
		int const len = strlen(addr_string.base());
		int const n   = write(fd, addr_string.base(), len);
		close(fd);
		if (n != len) return Errno(EIO);
	}

	return 0;
}


extern "C" int socket_fs_connect(int libc_fd, sockaddr const *addr, socklen_t addrlen)
{
	if (addr->sa_family != AF_INET) {
		Genode::error(__func__, ": family not supported");
		return Errno(EAFNOSUPPORT);
	}

	Libc::File_descriptor *fd = Libc::file_descriptor_allocator()->find_by_libc_fd(libc_fd);
	if (!fd)
		return Errno(EBADF);
	Socket_context *context;
	if (!(context = dynamic_cast<Socket_context *>(fd->context)))
		return Errno(ENOTSOCK);

	{
		Sockaddr_string addr_string(host_string(*(sockaddr_in const *)addr),
		                            port_string(*(sockaddr_in const *)addr));

		Absolute_path file("connect", context->path.base());
		int const fd = open(file.base(), O_WRONLY);
		if (fd == -1) {
			Genode::error(__func__, ": connect file not accessible");
			return Errno(EINVAL);
		}
		int const len = strlen(addr_string.base());
		int const n   = write(fd, addr_string.base(), len);
		close(fd);
		if (n != len) return Errno(EIO);
	}

	return 0;
}


extern "C" int socket_fs_listen(int libc_fd, int backlog)
{
	Libc::File_descriptor *fd = Libc::file_descriptor_allocator()->find_by_libc_fd(libc_fd);
	if (!fd)
		return Errno(EBADF);
	Socket_context *context;
	if (!(context = dynamic_cast<Socket_context *>(fd->context)))
		return Errno(ENOTSOCK);

	{
		Absolute_path file("listen", context->path.base());
		int const fd = open(file.base(), O_WRONLY);
		if (fd == -1) {
			Genode::error(__func__, ": listen file not accessible");
			return Errno(EINVAL);
		}
		char buf[10];
		snprintf(buf, sizeof(buf), "%d", backlog);
		int const n = write(fd, buf, strlen(buf));
		close(fd);
		if ((unsigned)n != strlen(buf)) return Errno(EIO);
	}

	return 0;
}


static ssize_t do_recvfrom(Libc::File_descriptor *fd,
                           void *buf, ::size_t len, int flags,
                           struct sockaddr *src_addr, socklen_t *src_addrlen)
{
	Socket_context *context = dynamic_cast<Socket_context *>(fd->context);
	if (!context)     return Errno(ENOTSOCK);
	if (!buf || !len) return Errno(EINVAL);

	/* TODO if "remote" is empty we have to block for the next packet */
	if (src_addr) {
		int const res = read_sockaddr_in(context->remote_fd(), context,
		                                 (sockaddr_in *)src_addr, src_addrlen,
		                                 true);
		if (res < 0) return res;
	}

	try {
		lseek(context->data_fd(), 0, 0);
		ssize_t out_len = read(context->data_fd(), buf, len);
		return out_len;
	} catch (Socket_context::Inaccessible) {
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
	Socket_context *context = dynamic_cast<Socket_context *>(fd->context);
	if (!context)     return Errno(ENOTSOCK);
	if (!buf || !len) return Errno(EINVAL);

	if (dest_addr) {
		Sockaddr_string addr_string(host_string(*(sockaddr_in const *)dest_addr),
		                            port_string(*(sockaddr_in const *)dest_addr));

#warning TODO use remote_fd()
		Absolute_path file("remote", context->path.base());
		int const fd = open(file.base(), O_WRONLY);
		if (fd == -1) {
			Genode::error(__func__, ": remote file not accessible");
			return Errno(EINVAL);
		}
		int const len = strlen(addr_string.base());
		int const n   = write(fd, addr_string.base(), len);
		close(fd);
		if (n != len) return Errno(EIO);
	}

	try {
		lseek(context->data_fd(), 0, 0);
		ssize_t out_len = write(context->data_fd(), buf, len);
		return out_len;
	} catch (Socket_context::Inaccessible) {
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
	Genode::warning("##########  TODO  ########## ", __func__);
	return 0;
}


extern "C" int socket_fs_setsockopt(int libc_fd, int level, int optname,
                                    void const *optval, socklen_t optlen)
{
	Genode::warning("##########  TODO  ########## ", __func__);
	return 0;
}


extern "C" int socket_fs_shutdown(int libc_fd, int how)
{
	/* TODO ENOTCONN */

	if (how != SHUT_RDWR) {
		Genode::error("function '", __func__ , "' only implemented for 'how",
		              "value 'SHUT_RDWR'");
	}

	Libc::File_descriptor *fd = Libc::file_descriptor_allocator()->find_by_libc_fd(libc_fd);
	if (!fd)
		return Errno(EBADF);
	Socket_context *context;
	if (!(context = dynamic_cast<Socket_context *>(fd->context)))
		return Errno(ENOTSOCK);

	unlink(context->path.base());

	return 0;
}


static Genode::String<16> new_socket(Absolute_path const &path)
{
	Absolute_path new_socket("new_socket", path.base());

	int const fd = open(new_socket.base(), O_RDONLY);
	if (fd == -1) {
		Genode::error(__func__, ": new_socket file not accessible - socket fs not mounted?");
		throw New_socket_failed();
	}
	char buf[10];
	int const n = read(fd, buf, sizeof(buf));
	close(fd);
	if (n == -1 || !n || n >= (int)sizeof(buf) - 1)
		throw New_socket_failed();
	buf[n] = 0;

	return Genode::String<16>(buf);
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
	try {
		Absolute_path proto_path(path);
		if (type == SOCK_STREAM)
			proto_path.append("/tcp");
		else
			proto_path.append("/udp");

		Genode::String<16> socket_path = new_socket(proto_path);
		path.append("/");
		path.append(socket_path.string());
	} catch (New_socket_failed) { return Errno(EINVAL); }

	Socket_context *context = new (&global_allocator)
	                          Socket_context(path);
	Libc::File_descriptor *fd =
		Libc::file_descriptor_allocator()->alloc(&socket_plugin(), context);

	return fd->libc_fd;
}


/****************************
 ** File-plugin operations **
 ****************************/

//int Socket_plugin::fcntl(Libc::File_descriptor *fd, int cmd, long arg) override;

ssize_t Socket_plugin::read(Libc::File_descriptor *fd, void *buf, ::size_t count)
{
	return do_recvfrom(fd, buf, count, 0, nullptr, nullptr);
}


ssize_t Socket_plugin::write(Libc::File_descriptor *fd, const void *buf, ::size_t count)
{
	return do_sendto(fd, buf, count, 0, nullptr, 0);
}


bool Socket_plugin::supports_select(int nfds,
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


int Socket_plugin::select(int nfds,
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
				Socket_context *context = dynamic_cast<Socket_context *>(fdo->context);
				if (context->data_read_ready()) {
					FD_SET(fd, readfds);
					++nready;
				}
			} catch (Socket_context::Inaccessible) { }
		}

		if (FD_ISSET(fd, &in_writefds)) {
			if (false /* XXX ask if "data" is writeable */) {
				FD_SET(fd, writefds);
				++nready;
			}
		}

		/* XXX exceptfds not supported */
	}

	return nready;
}


int Socket_plugin::close(Libc::File_descriptor *fd)
{
	Socket_context *context = dynamic_cast<Socket_context *>(fd->context);
	if (!context) return Errno(EBADF);

	::unlink(context->path.base());

	Genode::destroy(&global_allocator, context);
	Libc::file_descriptor_allocator()->free(fd);

	return 0;
}
