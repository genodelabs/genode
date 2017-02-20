/*
 * \brief  Socket interface for calling the Linux WIFI stack
 * \author Josef Soentgen
 * \date   2012-08-02
 */

/*
 * Copyright (C) 2014-2017 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

#ifndef _WIFI__SOCKET_CALL_H_
#define _WIFI__SOCKET_CALL_H_


namespace Wifi {
	struct Socket;
	struct Sockaddr;
	struct Msghdr;
	struct Socket_call;
	struct Poll_socket_fd;

	/*
	 * XXX We need the WIFI_ prefix because SOL_SOCKET and friends are
	 *     preprocessor macros.
	 */

	enum Flags { WIFI_F_NONE, WIFI_F_MSG_ERRQUEUE };
	enum Sockopt_level { WIFI_SOL_SOCKET, WIFI_SOL_NETLINK };
	enum Sockopt_name {
		/* SOL_SOCKET */
		WIFI_SO_SNDBUF,
		WIFI_SO_RCVBUF,
		WIFI_SO_PASSCRED,
		WIFI_SO_WIFI_STATUS,
		/* SOL_NETLINK */
		WIFI_NETLINK_ADD_MEMBERSHIP,
		WIFI_NETLINK_DROP_MEMBERSHIP,
		WIFI_NETLINK_PKTINFO,
	};

	enum Poll_mask {
		WIFI_POLLIN  = 0x1,
		WIFI_POLLOUT = 0x2,
		WIFI_POLLEX  = 0x4,
	};

	enum { MAX_POLL_SOCKETS = 16, };

	typedef __SIZE_TYPE__    size_t;
	typedef __PTRDIFF_TYPE__ ssize_t;
}


struct Wifi::Sockaddr { };
struct Wifi::Msghdr
{
	enum { MAX_IOV_LEN = 8 };

	struct Iov {
		void   *iov_base;
		size_t  iov_len;
	};

	void     *msg_name;
	unsigned  msg_namelen;
	Iov       msg_iov[MAX_IOV_LEN];
	unsigned  msg_iovlen;
	unsigned  msg_count;
	/* XXX recvmsg msg_flags ? */
	void     *msg_control;
	unsigned  msg_controllen;
};

struct Wifi::Socket_call
{
	Socket* socket(int domain, int type, int protocol);
	int close(Socket *);

	int bind(Socket *, Sockaddr const *addr, unsigned addrlen);
	int getsockname(Socket *, Sockaddr *addr, unsigned *addrlen);
	int poll_all(Poll_socket_fd *, unsigned, int);
	ssize_t recvmsg(Socket*, Msghdr *, Flags flags);
	ssize_t sendmsg(Socket*, Msghdr const *, Flags flags);
	int setsockopt(Socket*, Sockopt_level level, Sockopt_name optname,
	               void const *optval, unsigned optlen);

	void non_block(Socket *, bool);

	/**
	 * Special ioctl related functions
	 */
	void get_mac_address(unsigned char *addr);
};

struct Wifi::Poll_socket_fd
{
	Socket *s;
	void   *pfd;
	int     events;
	int     revents;
};

#endif /* _WIFI__SOCKET_CALL_H_ */
