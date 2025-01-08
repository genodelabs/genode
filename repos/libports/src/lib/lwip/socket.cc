/*
 * \brief  Implementation of Genode's socket C-API for lwip
 * \author Sebastian Sumpf
 * \date   2025-01-07
 *
 * All calls except 'genode_socket_config_address' are non-blocking.
 */

/*
 * Copyright (C) 2025 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2 or later.
 */

#include <timer_session/connection.h>

#include <lwip_genode_init.h>
#include <socket_lwip.h>

/*
 * Lwip headers provide extern "C", and thus, can be interfaced from C++ directly
 */
namespace Lwip {
#include <lwip/udp.h>
}


namespace Socket {
	struct Main;
}


struct Statics
{
	genode_netif_handle       *netif_ptr;
	genode_socket_wakeup      *wakeup_remote;
	genode_socket_io_progress *io_progress;
	Genode::Heap              *heap;
	Genode::Env               *env;
};


static Statics &statics()
{
	static Statics instance { };
	return instance;
}


Errno Socket::genode_errno(Lwip::err_t errno)
{
	switch (errno) {
	case Lwip::ERR_OK:         return GENODE_ENONE;
	case Lwip::ERR_MEM:        return GENODE_ENOMEM;
	case Lwip::ERR_TIMEOUT:    return GENODE_ETIMEDOUT;
	case Lwip::ERR_INPROGRESS: return GENODE_EINPROGRESS;
	case Lwip::ERR_VAL:        return GENODE_EINVAL;
	case Lwip::ERR_WOULDBLOCK: return GENODE_EAGAIN;
	case Lwip::ERR_USE:        return GENODE_EADDRINUSE;
	case Lwip::ERR_ISCONN:     return GENODE_EISCONN;
	case Lwip::ERR_CONN:       return GENODE_ENOTCONN;
	case Lwip::ERR_ABRT:       return GENODE_ECONNABORTED;
	case Lwip::ERR_RST:        return GENODE_ECONNRESET;
	default:
		Genode::error("unknown Lwip::err_t (", (int)errno, ")");
	}

	return GENODE_EINVAL;
}


struct Socket::Main
{
	Env  &_env;
	Heap  _heap { _env.pd(), _env.rm() };

	Timer::Connection _timer { _env, "vfs_lwip" };

	Io_signal_handler<Main> nic_client_handler { _env.ep(), *this,
		&Main::handle_nic_client };

	Io_signal_handler<Main> link_state_handler { _env.ep(), *this,
		&Main::handle_link_state };

	void _io_progress()
	{
		if (statics().io_progress && statics().io_progress->callback)
			statics().io_progress->callback(statics().io_progress->data);
	}

	Main(Env &env) : _env(env)
	{
		statics().heap = &_heap;

		genode_nic_client_init(genode_env_ptr(env),
		                       genode_allocator_ptr(_heap),
		                       genode_signal_handler_ptr(nic_client_handler),
		                       genode_signal_handler_ptr(link_state_handler));

		Lwip::genode_init(_heap, _timer);

		/* create lwIP-network interface */
		statics().netif_ptr = lwip_genode_netif_init();
	}

	void handle_nic_client()
	{
		lwip_genode_netif_rx(statics().netif_ptr);
		_io_progress();

	}

	void handle_link_state()
	{
		lwip_genode_netif_link_state(statics().netif_ptr);
		_io_progress();
	}
};


/*
 * Socket C-API
 */

struct genode_socket_handle
{
	Socket::Protocol &protocol;

	genode_socket_handle(Socket::Protocol &protocol)
	: protocol(protocol) { }
};


void genode_socket_config_address(struct genode_socket_config *config)
{
	lwip_genode_netif_address(statics().netif_ptr, config);

	/* block */
	while (!lwip_genode_netif_configured(statics().netif_ptr)) {
		genode_socket_wakeup_remote();
		genode_socket_wait_for_progress();
	}
}


void genode_socket_config_info(struct genode_socket_info *info)
{
	if (!info) return;

	lwip_genode_netif_info(statics().netif_ptr, info);
}


void genode_socket_configure_mtu(unsigned mtu)
{
	lwip_genode_netif_mtu(statics().netif_ptr, mtu);
}


void genode_socket_wakeup_remote(void)
{
	genode_nic_client_notify_peers();
}


void genode_socket_register_wakeup(struct genode_socket_wakeup *remote)
{
	statics().wakeup_remote = remote;
}


struct genode_socket_handle *
genode_socket(int domain, int type, int protocol, enum Errno *errno)
{
	*errno = GENODE_ENONE;

	if (domain != AF_INET) {
		*errno = GENODE_EAFNOSUPPORT;
		return nullptr;
	}

	if (!(type == SOCK_STREAM || type == SOCK_DGRAM)) {
		*errno = GENODE_EINVAL;
		return nullptr;
	}

	if (protocol) {
		*errno = GENODE_EPROTONOSUPPORT;
		return nullptr;
	}

	Socket::Protocol *proto = nullptr;

	Genode::Allocator &alloc = *statics().heap;

	if (type == SOCK_STREAM)
		proto = Socket::create_tcp(alloc);
	else
		proto = Socket::create_udp(alloc);

	return new (alloc) genode_socket_handle(*proto);
}


enum Errno genode_socket_release(struct genode_socket_handle *handle)
{
	destroy(statics().heap, &handle->protocol);
	destroy(statics().heap, handle);

	return GENODE_ENONE;
}

enum Errno genode_socket_bind(struct genode_socket_handle  *handle,
                              struct genode_sockaddr const *addr)
{
	return handle->protocol.bind(*addr);
}


enum Errno genode_socket_listen(struct genode_socket_handle *handle,
                                int backlog)
{
	return handle->protocol.listen(backlog & 0xff);
}


struct genode_socket_handle *
genode_socket_accept(struct genode_socket_handle *handle,
                     struct genode_sockaddr *addr,
                     enum Errno *errno)
{
	Socket::Protocol *proto =  handle->protocol.accept(addr, *errno);

	/* errno should be set */
	if (proto == nullptr)
		return nullptr;

	return new (statics().heap) genode_socket_handle(*proto);
}


enum Errno genode_socket_connect(struct genode_socket_handle *handle,
                                 struct genode_sockaddr *addr)
{
	/* udp */
	bool unspec = addr->family == AF_UNSPEC;
	return handle->protocol.connect(*addr, unspec);
}


enum Errno genode_socket_sendmsg(struct genode_socket_handle *handle,
                                 struct genode_msghdr *msg,
                                 unsigned long *bytes_send)
{
	return handle->protocol.sendmsg(*msg, *bytes_send);
}


enum Errno genode_socket_recvmsg(struct genode_socket_handle *handle,
                                 struct genode_msghdr *msg,
                                 unsigned long *bytes_recv,
                                 bool msg_peek)
{
	return handle->protocol.recvmsg(*msg, *bytes_recv, msg_peek);
}


unsigned genode_socket_pollin_set(void) {
	return Socket::Protocol::Poll::READ; }


unsigned genode_socket_pollout_set(void) {
	return Socket::Protocol::Poll::WRITE; }


unsigned genode_socket_pollex_set(void) {
	return Socket::Protocol::Poll::EXCEPTION; }


unsigned genode_socket_poll(struct genode_socket_handle *handle)
{
	return handle->protocol.poll();
}


enum Errno genode_socket_setsockopt(struct genode_socket_handle *,
                                    enum Sock_level, enum Sock_opt,
                                    void const *,
                                    unsigned)
{
	return GENODE_ENOPROTOOPT;
}


enum Errno genode_socket_getsockopt(struct genode_socket_handle *handle,
                                    enum Sock_level level, enum Sock_opt opt,
                                    void *optval, unsigned *optlen)
{
	if (level != GENODE_SOL_SOCKET) {
		Genode::error("getsockopt: unsupported level (", (unsigned)level, ")");
		return GENODE_ENOPROTOOPT;
	}

	switch (opt) {
	case GENODE_SO_ERROR:
		if (!optlen || *optlen < sizeof(Errno)) return GENODE_EFAULT;
		*(unsigned *)optval = handle->protocol.so_error();
		return GENODE_ENONE;
	default:
		Genode::warning("getsockopt: unsupported option (", (unsigned)opt, ")");
		return GENODE_ENOPROTOOPT;
	}
}


enum Errno genode_socket_shutdown(struct genode_socket_handle *handle,
                                  int)
{
	handle->protocol.shutdown();
	return GENODE_ENONE;
}


enum Errno genode_socket_getsockname(struct genode_socket_handle *handle,
                                     struct genode_sockaddr *addr)
{
	return handle->protocol.name(*addr);
}


enum Errno genode_socket_getpeername(struct genode_socket_handle *handle,
                                     struct genode_sockaddr *addr)
{
	return handle->protocol.peername(*addr);
}


void genode_socket_wait_for_progress()
{
	if (statics().env)
		statics().env->ep().wait_and_dispatch_one_io_signal();
}


void genode_socket_init(struct genode_env *_env,
                        struct genode_socket_io_progress *io_progress)
{
	Genode::Env &env = *static_cast<Genode::Env *>(_env);

	statics().env         = &env;
	statics().io_progress = io_progress;

	static Socket::Main main { env };
}


/*
 * Callbacks of Socket C-API
 */

void lwip_genode_socket_schedule_peer(void)
{
	if (statics().wakeup_remote && statics().wakeup_remote->callback) {
		statics().wakeup_remote->callback(statics().wakeup_remote->data);
	}
}
