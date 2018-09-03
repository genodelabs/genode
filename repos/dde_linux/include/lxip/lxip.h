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

#endif /* _INCLUDE_LXIP_LXIP_H_ */
