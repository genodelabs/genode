/*
 * \brief  lwIP protocol bindings
 * \author Sebastian Sumpf
 * \date   2025-02-27
 */

/*
 * Copyright (C) 2025 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__SOCKET_LWIP_H_
#define _INCLUDE__SOCKET_LWIP_H_

#include <base/heap.h>

#include <genode_c_api/nic_client.h>
#include <genode_c_api/socket.h>

#include <nic_netif.h>

namespace Lwip {
#include <lwip/err.h>
#include <lwip/ip_addr.h>
}

namespace Socket {
	using namespace Genode;
	struct Protocol;

	Errno genode_errno(Lwip::err_t errno);

	Protocol *create_tcp(Allocator &);
	Protocol *create_udp(Allocator &);
}


struct Socket::Protocol : private Noncopyable
{
	enum State {
		NEW     = 0,
		BOUND   = 1,
		CONNECT = 2,
		LISTEN  = 3,
		READY   = 4,
		CLOSING = 5,
		CLOSED  = 6,
	};

	enum Poll {
		NONE      = 0u,
		READ      = 1u,
		WRITE     = 1u << 1,
		EXCEPTION = 1u << 2,
	};

	State _state;
	Errno _so_error { GENODE_ENONE };
	Errno _connect_error { GENODE_ENONE };

	Protocol(State state) : _state(state) { };

	virtual ~Protocol() { }

	virtual Errno     bind(genode_sockaddr const &) = 0;
	virtual Errno     listen(uint8_t) = 0;
	virtual Protocol *accept(genode_sockaddr *, Errno &) = 0;
	virtual Errno     connect(genode_sockaddr const &, bool disconnect = false) = 0;
	virtual Errno     sendmsg(genode_msghdr &, unsigned long &) = 0;
	virtual Errno     recvmsg(genode_msghdr &, unsigned long &, bool) = 0;
	virtual Errno     peername(genode_sockaddr &addr) = 0;
	virtual Errno     name(genode_sockaddr &addr) = 0;
	virtual unsigned  poll() = 0;
	virtual Errno     shutdown() = 0;
	virtual Errno     getsockopt(Sock_level, Sock_opt, void *, unsigned *) = 0;
	virtual Errno     setsockopt(Sock_level, Sock_opt, void const *, unsigned) = 0;

	Errno so_error()
	{
		Errno ret = _so_error;
		_so_error = GENODE_ENONE;
		return ret;
	}

	Errno lwip_error(Errno err)
	{
		_so_error = err;
		return err;
	}

	Errno lwip_error(Lwip::err_t err)
	{
		_so_error = genode_errno(err);
		return _so_error;
	}

	template<typename F>
	void for_each_iovec(genode_msghdr &hdr, F const &fn)
	{
		for (unsigned long i = 0; i < hdr.iovlen; i++) {
			genode_iovec &iov = hdr.iov[i];
			fn(iov.base, iov.size, iov.used);
		}
	}

	template<typename F>
	void for_each_64k_chunk(void *_base, size_t size, F const &fn )
	{
		uint8_t *base = static_cast<uint8_t *>(_base);

		while (size) {
			uint16_t chunk_size = size > uint16_t(~0u) ? uint16_t(~0u) : size;
			fn(base, chunk_size);
			size -= chunk_size;
			base += chunk_size;
		}
	}

	Lwip::ip_addr_t lwip_ip_addr(genode_sockaddr const &addr)
	{
		using namespace Lwip;
		/* is macro */
		return IPADDR4_INIT(addr.in.addr);
	}
};

#endif /* _INCLUDE__SOCKET_LWIP_H_ */
