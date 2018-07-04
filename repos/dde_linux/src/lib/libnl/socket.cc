/*
 * \brief  Linux emulation code
 * \author Josef Soentgen
 * \date   2014-07-28
 */

/*
 * Copyright (C) 2014-2017 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

/* Genode includes */
#include <base/env.h>
#include <base/log.h>
#include <base/snprintf.h>
#include <util/list.h>
#include <util/string.h>
#include <util/misc_math.h>

/* Libc includes */
#include <unistd.h>
#include <errno.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <sys/poll.h>

#include <wifi/socket_call.h>

extern "C" {
#include <libnl_emul.h>
#include <linux/netlink.h>
}

/* from netlink-private/netlink.h */
#ifndef SOL_NETLINK
#define SOL_NETLINK 270
#endif

/* uapi/asm-generic/socket.h */
#ifndef SO_WIFI_STATUS
#define SO_WIFI_STATUS 41
#endif

/* bits/socket.h */
#ifndef MSG_ERRQUEUE
#define MSG_ERRQUEUE 0x2000
#endif


static const bool trace = true;
#define TRACE() \
	do { if (trace) \
			Genode::log("called from: ", __builtin_return_address(0)); \
	} while (0)


using namespace Wifi;

extern Socket_call socket_call;


struct Socket_fd
{
	Socket *s;
	int     fd;
};


class Socket_registry
{
	private :

		enum {
			/* lower FDs might be special */
			SOCKETS_OFFSET_VALUE = 100,
			MAX_SOCKETS = 7,
		};

		static Socket_fd _socket_fd[MAX_SOCKETS];
		static unsigned _sockets;

		template <typename FUNC>
		static void _for_each_socket_fd(FUNC const & func)
		{
			for (int i = 0; i < MAX_SOCKETS; i++) {
				if (func(_socket_fd[i])) { break; }
			}
		}

	public:

		static int insert(Socket *s)
		{
			int fd = -1;

			auto lambda = [&] (Socket_fd &sfd) {
				if (sfd.s != nullptr) { return false; }

				sfd.s  = s;
				sfd.fd = (++_sockets & 0xff) + SOCKETS_OFFSET_VALUE;

				/* return fd */
				fd = sfd.fd;
				return true;
			};

			_for_each_socket_fd(lambda);

			return fd;
		}

		static void remove(Socket *s)
		{
			auto lambda = [&] (Socket_fd &sfd) {
				if (sfd.s != s) { return false; }

				sfd.s  = nullptr;
				sfd.fd = 0;

				return true;
			};

			_for_each_socket_fd(lambda);
		}

		static Socket *find(int fd)
		{
			Socket *s = nullptr;

			auto lambda = [&] (Socket_fd &sfd) {
				if (sfd.fd != fd) { return false; }

				/* return socket */
				s = sfd.s;
				return true;
			};

			_for_each_socket_fd(lambda);

			return s;
		}
};

Socket_fd Socket_registry::_socket_fd[MAX_SOCKETS] = {};
unsigned Socket_registry::_sockets = 0;


extern "C" {

/******************
 ** sys/socket.h **
 ******************/

int bind(int sockfd, const struct sockaddr *addr, socklen_t addrlen)
{
	Socket *s = Socket_registry::find(sockfd);
	if (!s) {
		errno = EBADF;
		return -1;
	}

	/* FIXME convert to/from Sockaddr */
	int const err = socket_call.bind(s, (Wifi::Sockaddr const *)addr, addrlen);
	if (err < 0) {
		errno = -err;
		return -1;
	}

	return err;
}


int getsockname(int sockfd, struct sockaddr *addr, socklen_t *addrlen)
{
	Socket *s = Socket_registry::find(sockfd);
	if (!s) {
		errno = EBADF;
		return -1;
	}

	/* FIXME convert to/from Sockaddr */
	int const err = socket_call.getsockname(s, (Wifi::Sockaddr *)addr, addrlen);
	if (err < 0) {
		errno = -err;
		return -1;
	}

	return err;
}


ssize_t recvfrom(int sockfd, void *buf, size_t len, int flags,
                 struct sockaddr *src_addr, socklen_t *addrlen)
{
	Socket *s = Socket_registry::find(sockfd);
	if (!s) {
		Genode::error("sockfd ", sockfd, " not in registry");
		errno = EBADF;
		return -1;
	}

	Wifi::Msghdr w_msg;

	w_msg.msg_name            = (void*)src_addr;
	w_msg.msg_namelen         = *addrlen;
	w_msg.msg_iovlen          = 1;
	w_msg.msg_iov[0].iov_base = buf;
	w_msg.msg_iov[0].iov_len  = len;
	w_msg.msg_count           = len;

	/* FIXME convert to/from Sockaddr */
	/* FIXME flags values */
	int const err = socket_call.recvmsg(s, &w_msg, Wifi::WIFI_F_NONE);
	if (err < 0) {
		errno = -err;
		return -1;
	}

	return err;
}


ssize_t recvmsg(int sockfd, struct msghdr *msg, int flags)
{
	Socket *s = Socket_registry::find(sockfd);
	if (!s) {
		errno = EBADF;
		return -1;
	}

	if (msg->msg_iovlen > Wifi::Msghdr::MAX_IOV_LEN) {
		Genode::error(__func__, ": ", msg->msg_iovlen, " exceeds maximum iov "
		              "length (", (int)Wifi::Msghdr::MAX_IOV_LEN, ")");
		errno = EINVAL;
		return -1;
	}

	/* XXX support multiple flags */
	Wifi::Flags w_flags = Wifi::WIFI_F_NONE;
	if (flags & MSG_ERRQUEUE)
		w_flags = Wifi::WIFI_F_MSG_ERRQUEUE;

	Wifi::Msghdr w_msg;

	w_msg.msg_name    = msg->msg_name;
	w_msg.msg_namelen = msg->msg_namelen;
	w_msg.msg_iovlen  = msg->msg_iovlen;
	for (unsigned i = 0; i < w_msg.msg_iovlen; ++i) {
		w_msg.msg_iov[i].iov_base = msg->msg_iov[i].iov_base;
		w_msg.msg_iov[i].iov_len  = msg->msg_iov[i].iov_len;
		w_msg.msg_count          += msg->msg_iov[i].iov_len;
	}

	w_msg.msg_control    = msg->msg_control;
	w_msg.msg_controllen = msg->msg_controllen;

	int const err = socket_call.recvmsg(s, &w_msg, w_flags);

	if (err < 0) {
		errno = -err;
		return -1;
	}

	if (err > 0 && msg->msg_name)
		msg->msg_namelen = w_msg.msg_namelen;

	return err;
}


ssize_t send(int sockfd, const void *buf, size_t len, int flags)
{
	return sendto(sockfd, buf, len, flags, 0, 0);
}


ssize_t sendmsg(int sockfd, const struct msghdr *msg, int flags)
{
	Socket *s = Socket_registry::find(sockfd);
	if (!s) {
		errno = EBADF;
		return -1;
	}

	if (msg->msg_iovlen > Wifi::Msghdr::MAX_IOV_LEN) {
		Genode::error(__func__, ": ", msg->msg_iovlen, " exceeds maximum iov "
		              "length (", (int)Wifi::Msghdr::MAX_IOV_LEN, ")");
		errno = EINVAL;
		return -1;
	}
	if (msg->msg_controllen != 0) {
		Genode::error(__func__, ": msg_control not supported");
		errno = EINVAL;
		return -1;
	}
	if (flags != 0) {
		Genode::error(__func__, ": flags not supported");
		errno = EOPNOTSUPP;
		return -1;
	}

	Wifi::Msghdr w_msg;

	w_msg.msg_name    = msg->msg_name;
	w_msg.msg_namelen = msg->msg_namelen;
	w_msg.msg_iovlen  = msg->msg_iovlen;
	for (unsigned i = 0; i < w_msg.msg_iovlen; ++i) {
		w_msg.msg_iov[i].iov_base = msg->msg_iov[i].iov_base;
		w_msg.msg_iov[i].iov_len  = msg->msg_iov[i].iov_len;
		w_msg.msg_count          += msg->msg_iov[i].iov_len;
	}

	int const err = socket_call.sendmsg(s, &w_msg, Wifi::WIFI_F_NONE);

	if (err < 0) {
		errno = -err;
		return -1;
	}

	return err;
}


ssize_t sendto(int sockfd, const void *buf, size_t len, int flags,
               const struct sockaddr *dest_addr, socklen_t addrlen)
{
	Socket *s = Socket_registry::find(sockfd);
	if (!s)
		return -1;

	Wifi::Msghdr w_msg;

	w_msg.msg_name            = (void*)dest_addr;
	w_msg.msg_namelen         = addrlen;
	w_msg.msg_iovlen          = 1;
	w_msg.msg_iov[0].iov_base = const_cast<void*>(buf);
	w_msg.msg_iov[0].iov_len  = len;
	w_msg.msg_count           = len;

	/* FIXME convert to/from Sockaddr */
	/* FIXME flags values */
	int const err = socket_call.sendmsg(s, &w_msg, Wifi::WIFI_F_NONE);
	if (err < 0) {
		errno = -err;
		return -1;
	}

	return err;
}


namespace { struct Invalid_arg { }; }

static Sockopt_level sockopt_level(int const in)
{
	switch (in) {
	case SOL_SOCKET:  return Wifi::WIFI_SOL_SOCKET;
	case SOL_NETLINK: return Wifi::WIFI_SOL_NETLINK;
	default:          throw Invalid_arg();
	}
}


static Sockopt_name sockopt_name(int const level, int const in)
{
	switch (level) {
	case SOL_SOCKET:
		switch (in) {
		case SO_SNDBUF:      return Wifi::WIFI_SO_SNDBUF;
		case SO_RCVBUF:      return Wifi::WIFI_SO_RCVBUF;
		case SO_PASSCRED:    return Wifi::WIFI_SO_PASSCRED;
		case SO_WIFI_STATUS: return Wifi::WIFI_SO_WIFI_STATUS;
		default:             throw Invalid_arg();
		}
	case SOL_NETLINK:
		switch (in) {
		case NETLINK_ADD_MEMBERSHIP:  return Wifi::WIFI_NETLINK_ADD_MEMBERSHIP;
		case NETLINK_DROP_MEMBERSHIP: return Wifi::WIFI_NETLINK_DROP_MEMBERSHIP;
		case NETLINK_PKTINFO:         return Wifi::WIFI_NETLINK_PKTINFO;
		default:                      throw Invalid_arg();
		}
	default: throw Invalid_arg();
	}
}


int setsockopt(int sockfd, int level, int optname, const void *optval,
               socklen_t optlen)
{
	Socket *s = Socket_registry::find(sockfd);
	if (!s)
		return -1;

	try {
		/* FIXME optval values */
		int const err =
			socket_call.setsockopt(s,
		                           sockopt_level(level),
		                           sockopt_name(level, optname),
		                           optval, optlen);
		if (err < 0) {
			errno = -err;
			return 1;
		}
	} catch (Invalid_arg) { return -1; }

	return 0;
}


int socket(int domain, int type, int protocol)
{
	/* FIXME domain, type, protocol values */
	Socket *s = socket_call.socket(domain, type, protocol);

	if (!s)
		return -1;

	return Socket_registry::insert(s);
}


/**************
 ** unistd.h **
 **************/

int close(int fd)
{
	Socket *s = Socket_registry::find(fd);
	if (!s)
		return -1;

	Socket_registry::remove(s); /* XXX all further operations on s shall fail */

	int const err = socket_call.close(s);
	if (err < 0) {
		errno = -err;
		return -1;
	}

	return err;
}


/*************
 ** fnctl.h **
 *************/

int fcntl(int fd, int cmd, ... /* arg */ )
{
	Socket *s = Socket_registry::find(fd);
	if (!s)
		return -1;

	long arg;

	va_list ap;
	va_start(ap, cmd);
	arg = va_arg(ap, long);
	va_end(ap);

	switch (cmd) {
	case F_SETFL:
		if (arg == O_NONBLOCK) {
			socket_call.non_block(s, true);
			return 0;
		}
	default:
		Genode::warning("fcntl: unknown request: ", cmd);
		break;
	}

	return -1;
}


/****************
 ** sys/poll.h **
 ****************/

static bool _ctrl_fd_set = false;


extern "C" void nl_set_wpa_ctrl_fd()
{
	_ctrl_fd_set = true;
}

static bool special_fd(int fd)
{
	/*
	 * This range is used by the CTRL and RFKILL fds.
	 */
	return (fd > 40 && fd < 60);
}

int poll(struct pollfd *fds, nfds_t nfds, int timeout)
{
	Poll_socket_fd sockets[Wifi::MAX_POLL_SOCKETS];
	unsigned num = 0;
	int nready = 0;

	/* handle special FDs first */
	for (nfds_t i = 0; i < nfds; i++) {
		if (!special_fd(fds[i].fd)) { continue; }

		fds[i].revents  = 0;
		fds[i].revents |= POLLIN;
		nready++;
	}

	for (nfds_t i = 0; i < nfds; i++) {
		Socket *s = Socket_registry::find(fds[i].fd);
		if (!s) continue;

		Genode::memset(&sockets[num], 0, sizeof(Poll_socket_fd));

		sockets[num].s   = s;
		sockets[num].pfd = &fds[i];

		if (fds[i].events & POLLIN)
			sockets[num].events |= Wifi::WIFI_POLLIN;
		if (fds[i].events & POLLOUT)
			sockets[num].events |= Wifi::WIFI_POLLOUT;
		if (fds[i].events & POLLPRI)
			sockets[num].events |= Wifi::WIFI_POLLEX;

		num++;
	}

	/* make sure we do not block in poll_all */
	if (_ctrl_fd_set) {
		_ctrl_fd_set = false;
		timeout = 0;
	}

	int sready = socket_call.poll_all(sockets, num, timeout);
	if (sready < 0 || sready == 0)  { return nready; }

	nready += sready;

	for (unsigned i = 0; i < num; i++) {
		int revents  = sockets[i].revents;
		struct pollfd *pfd = static_cast<struct pollfd*>(sockets[i].pfd);

		pfd->revents = 0;
		if (revents & Wifi::WIFI_POLLIN)
			pfd->revents |= POLLIN;
		if (revents & Wifi::WIFI_POLLOUT)
			pfd->revents |= POLLOUT;
		if (revents & Wifi::WIFI_POLLEX)
			pfd->revents |= POLLPRI;
	}

	return nready;
}

} /* extern "C" */
