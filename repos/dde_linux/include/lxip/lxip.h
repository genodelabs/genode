/*
 * \brief  Lxip: Linux TCP/IP as a library
 * \author Christian Helmuth
 * \author Sebastian Sumpf
 * \date   2013-09-04
 */

/*
 * Copyright (C) 2012-2017 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

#ifndef _INCLUDE_LXIP_LXIP_H_
#define _INCLUDE_LXIP_LXIP_H_

#include <base/stdint.h>

namespace Lxip {

	struct Handle
	{
		void *socket;
		bool non_block;

		Handle() : socket(0), non_block(false) { }
	};

	enum Type { TYPE_STREAM, TYPE_DGRAM };

	class Socketcall;

	/**
	 * Init backend
	 *
	 * \param  ip_addr_str     IP address
	 * \param  netmask_str     Netmask
	 * \param  gateway_str     Gateway
	 * \param  nameserver_str  Nameserver
	 *
	 * \return Reference to Socketcall object
	 */
	Socketcall & init(Genode::Env &env,
	                  char const  *ip_addr_str,
	                  char const  *netmask_str,
	                  char const  *gateway_str,
	                  char const  *nameserver_str);

	typedef Genode::uint8_t  uint8_t;
	typedef Genode::uint16_t uint16_t;
	typedef Genode::uint32_t uint32_t;
	typedef signed long      ssize_t;
	typedef Genode::size_t   size_t;

	enum Poll_mask {
		POLLIN  = 0x1,
		POLLOUT = 0x2,
		POLLEX  = 0x4,
	};

	enum Message_flags {
		LINUX_MSG_COMPAT    = 0x0,
		LINUX_MSG_OOB       = 0x1,
		LINUX_MSG_PEEK      = 0x2,
		LINUX_MSG_DONTROUTE = 0x4,
		LINUX_MSG_CTRUNC    = 0x8,
		LINUX_MSG_TRUNC     = 0x20,
		LINUX_MSG_DONTWAIT  = 0x40,
		LINUX_MSG_EOR       = 0x80,
		LINUX_MSG_WAITALL   = 0x100,
		LINUX_MSG_EOF       = 0x200,
		LINUX_MSG_NOSIGNAL  = 0x4000,
	};

	enum Socket_level {
		LINUX_SOL_SOCKET = 1,
	};

	enum Ioctl_cmd {
		LINUX_FIONREAD = 0x541b, /* == SIOCINQ */
		LINUX_IFADDR   = 0x8915, /* == SIOCGIFADDR */
	};

	/*
	 * Must match errno values from lx_emul.h
	*/
	enum Io_result {
		LINUX_EAGAIN      = -35,
		LINUX_EINPROGRESS = -36,
		LINUX_EALREADY    = -37,
		LINUX_EISCONN     = -56,
	};
}


class Lxip::Socketcall
{
	public:

		virtual Handle  accept(Handle h, void *addr, uint32_t *len) = 0;
		virtual int     bind(Handle h, uint16_t family, void *addr) = 0;
		virtual void    close(Handle h) = 0;
		virtual int     connect(Handle h, uint16_t family, void *addr) = 0;
		virtual int     getpeername(Handle h, void *addr, uint32_t *len) = 0;
		virtual int     getsockname(Handle h, void *addr, uint32_t *len) = 0;
		virtual int     getsockopt(Handle h, int level, int optname,
		                           void *optval, int *optlen) = 0;
		virtual int     ioctl(Handle h, int request, char *arg) = 0;
		virtual int     listen(Handle h, int backlog) = 0;
		virtual int     poll(Handle h, bool block) = 0;
		virtual ssize_t recv(Handle h, void *buf, size_t len, int flags,
		                     uint16_t family, void *addr, uint32_t *addr_len) = 0;
		virtual ssize_t send(Handle h, const void *buf, size_t len, int flags,
		                     uint16_t family, void *addr) = 0;
		virtual int     setsockopt(Handle h, int level, int optname,
		                           const void *optval, uint32_t optlen) = 0;
		virtual int     shutdown(Handle h, int how) = 0;
		virtual Handle  socket(Type) = 0;
};

#endif /* _INCLUDE_LXIP_LXIP_H_ */
