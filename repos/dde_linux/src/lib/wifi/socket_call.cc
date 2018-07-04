/*
 * \brief  Linux emulation code
 * \author Josef Soentgen
 * \date   2014-08-04
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

/* local includes */
#include <lx.h>
#include <lx_emul.h>

#include <lx_emul/extern_c_begin.h>
# include <linux/socket.h>
# include <linux/net.h>
# include <net/sock.h>
#include <lx_emul/extern_c_end.h>

#include <wifi/socket_call.h>


static void run_socketcall(void *);


/* XXX move Wifi::Socket definition to better location */
struct Wifi::Socket
{
	void *socket    = nullptr;
	bool  non_block = false;

	Socket() { }

	explicit Socket(void *s) : socket(s) { }
};


struct Call
{
	enum Opcode {
		NONE, SOCKET, CLOSE,
		BIND, GETSOCKNAME, RECVMSG, SENDMSG, SENDTO, SETSOCKOPT,
		GET_MAC_ADDRESS, POLL_ALL, NON_BLOCK,
	};

	Opcode        opcode = NONE;
	Wifi::Socket *handle = nullptr;

	union {
		struct
		{
			int   domain;
			int   type;
			int   protocol;
			void *result;
		} socket;
		struct { /* no args */ } close;
		struct
		{
			sockaddr const *addr;
			int             addrlen;
		} bind;
		struct
		{
			sockaddr  *addr;
			int       *addrlen;
		} getsockname;
		struct
		{
			msghdr msg;
			int    flags;
			iovec  iov[Wifi::Msghdr::MAX_IOV_LEN];
		} recvmsg;
		struct
		{
			msghdr msg;
			int    flags;
			iovec  iov[Wifi::Msghdr::MAX_IOV_LEN];
		} sendmsg;
		struct {
			int         level;
			int         optname;
			void const *optval;
			uint32_t    optlen;
		} setsockopt;
		struct
		{
			unsigned char *addr;
		} get_mac_address;
		struct
		{
			Wifi::Poll_socket_fd *sockets;
			unsigned              num;
			int                   timeout;
		} poll_all;
		struct
		{
			bool value;
		} non_block;
	};

	int err = 0;
};

static Call              _call;
static Genode::Semaphore _block;


namespace Lx {
	class Socket;
}


/**
 * Context for socket calls
 */
class Lx::Socket
{
	private:


		Genode::Signal_transmitter            _sender;
		Genode::Signal_handler<Lx::Socket> _dispatcher;
		Lx::Task                           _task;

		struct socket *_call_socket()
		{
			struct socket *sock = static_cast<struct socket*>(_call.handle->socket);
			if (!sock)
				Genode::error("BUG: sock is zero");

			return sock;
		}

		void _do_socket()
		{
			struct socket *s;
			int res = sock_create_kern(nullptr, _call.socket.domain, _call.socket.type,
			                           _call.socket.protocol, &s);
			if (!res) {
				_call.socket.result = s;
				_call.err           = 0;
				return;
			}

			Genode::error("sock_create_kern failed, res: ", res);
			_call.socket.result = nullptr;
			_call.err           = res;
		}

		void _do_close()
		{
			struct socket *sock = _call_socket();

			_call.err = sock->ops->release(sock);
		}

		void _do_bind()
		{
			struct socket *sock = _call_socket();

			_call.err = sock->ops->bind(sock,
			                            const_cast<struct sockaddr *>(_call.bind.addr),
			                            _call.bind.addrlen);
		}

		void _do_getsockname()
		{
			struct socket *sock    = _call_socket();
			int            addrlen = *_call.getsockname.addrlen;

			_call.err = sock->ops->getname(sock, _call.getsockname.addr, &addrlen, 0);

			*_call.getsockname.addrlen = addrlen;
		}

		void _do_recvmsg()
		{
			struct socket *sock = _call_socket();
			struct msghdr *msg  = &_call.recvmsg.msg;

			if (_call.handle->non_block)
				msg->msg_flags |= MSG_DONTWAIT;

			size_t iovlen = 0;
			for (size_t i = 0; i < msg->msg_iter.nr_segs; i++)
				iovlen += msg->msg_iter.iov[i].iov_len;

			_call.err = sock->ops->recvmsg(sock, msg, iovlen, _call.recvmsg.flags);
		}

		void _do_sendmsg()
		{
			struct socket *sock = _call_socket();
			struct msghdr *msg  = const_cast<msghdr *>(&_call.sendmsg.msg);

			if (_call.handle->non_block)
				msg->msg_flags |= MSG_DONTWAIT;

			size_t iovlen = 0;
			for (size_t i = 0; i < msg->msg_iter.nr_segs; i++)
				iovlen += msg->msg_iter.iov[i].iov_len;

			_call.err = sock->ops->sendmsg(sock, msg, iovlen);
		}

		void _do_setsockopt()
		{
			struct socket *sock = _call_socket();

			/* taken from kernel_setsockopt() in net/socket.c */
			if (_call.setsockopt.level == SOL_SOCKET)
				_call.err = sock_setsockopt(sock,
				                            _call.setsockopt.level,
				                            _call.setsockopt.optname,
				                            (char *)_call.setsockopt.optval,
				                            _call.setsockopt.optlen);
			else
				_call.err = sock->ops->setsockopt(sock,
				                                 _call.setsockopt.level,
				                                 _call.setsockopt.optname,
				                                 (char *)_call.setsockopt.optval,
				                                 _call.setsockopt.optlen);
		}

		void _do_get_mac_address()
		{
			Lx::get_mac_address(_call.get_mac_address.addr);
		}

		void _do_poll_all()
		{
			Wifi::Poll_socket_fd *sockets = _call.poll_all.sockets;
			unsigned num                  = _call.poll_all.num;
			int timeout                   = _call.poll_all.timeout;

			enum {
				POLLIN_SET  = (POLLRDNORM | POLLRDBAND | POLLIN | POLLHUP | POLLERR),
				POLLOUT_SET = (POLLWRBAND | POLLWRNORM | POLLOUT | POLLERR),
				POLLEX_SET  = (POLLPRI)
			};

			int nready             = 0;
			bool timeout_triggered = false;
			bool woken_up          = false;
			do {
				/**
				 * Timeout was triggered, exit early.
				 */
				if (timeout_triggered)
					break;

				/**
				 * Poll each socket and check if there is something of interest.
				 */
				for (unsigned i = 0; i < num; i++) {
					struct socket *sock = static_cast<struct socket*>(sockets[i].s->socket);

					int mask = sock->ops->poll(0, sock, 0);

					sockets[i].revents = 0;
					if (mask & POLLIN_SET)
						sockets[i].revents |= sockets[i].events & Wifi::WIFI_POLLIN ? Wifi::WIFI_POLLIN : 0;
					if (mask & POLLOUT_SET)
						sockets[i].revents |= sockets[i].events & Wifi::WIFI_POLLOUT ? Wifi::WIFI_POLLOUT : 0;
					if (mask & POLLEX_SET)
						sockets[i].revents |= sockets[i].events & Wifi::WIFI_POLLEX ? Wifi::WIFI_POLLEX : 0;

					if (sockets[i].revents)
						nready++;
				}

				/**
				 * We were woken up but there is still nothing of interest.
				 */
				if (woken_up)
					break;

				/**
				 * Exit the loop if either a socket is ready or there is
				 * no timeout given.
				 */
				if (nready || !timeout)
					break;

				/**
				 * In case of a timeout add all sockets to an artificial wait list
				 * so at least one is woken up an sk_data_ready() call.
				 */
				Lx::Task *task = Lx::scheduler().current();
				struct socket_wq wq[num];
				Lx::Task::List wait_list;

				task->wait_enqueue(&wait_list);
				for (unsigned i = 0; i < num; i++) {
					struct socket *sock = static_cast<struct socket*>(sockets[i].s->socket);
					wq[i].wait.list = &wait_list;
					sock->sk->sk_wq = &wq[i];
				}

				long t = msecs_to_jiffies(timeout);
				timeout_triggered = !schedule_timeout(t);

				task->wait_dequeue(&wait_list);
				for (unsigned i = 0; i < num; i++) {
					struct socket *sock = static_cast<struct socket*>(sockets[i].s->socket);
					sock->sk->sk_wq = 0;
				}

				woken_up = true;
			} while (1);

			_call.err = nready;
		}

		void _do_non_block()
		{
			_call.handle->non_block = _call.non_block.value;
		}

		void _handle()
		{
			_task.unblock();
			Lx::scheduler().schedule();
		}

	public:

		Socket(Genode::Entrypoint &ep)
		:
			_dispatcher(ep, *this, &Lx::Socket::_handle),
			_task(run_socketcall, nullptr, "socketcall",
			      Lx::Task::PRIORITY_0, Lx::scheduler())
		{
			_sender.context(_dispatcher);
		}

		void exec_call()
		{
			switch (_call.opcode) {
			case Call::BIND:            _do_bind();            break;
			case Call::CLOSE:           _do_close();           break;
			case Call::GETSOCKNAME:     _do_getsockname();     break;
			case Call::POLL_ALL:        _do_poll_all();        break;
			case Call::RECVMSG:         _do_recvmsg();         break;
			case Call::SENDMSG:         _do_sendmsg();         break;
			case Call::SETSOCKOPT:      _do_setsockopt();      break;
			case Call::SOCKET:          _do_socket();          break;
			case Call::GET_MAC_ADDRESS: _do_get_mac_address(); break;
			case Call::NON_BLOCK:       _do_non_block();       break;

			default:
				Genode::warning("unknown opcode: ", (int)_call.opcode);
			case Call::NONE: /* ignore silently */
				_call.err = -EINVAL;
				break;
			}

			_call.opcode = Call::NONE;
			_block.up();
		}

		void submit_and_block()
		{
			_sender.submit();
			_block.down();
		}

		void unblock_task()
		{
			_task.unblock();
		}
};


static Lx::Socket *_socket;
static Genode::Allocator *_alloc;


void Lx::socket_init(Genode::Entrypoint &ep, Genode::Allocator &alloc)
{
	static Lx::Socket socket_ctx(ep);
	_socket = &socket_ctx;
	_alloc = &alloc;
}


void Lx::socket_kick()
{
	/* ignore silently, the function might be called to before init */
	if (!_socket) { return; }

	_socket->unblock_task();
}


static void run_socketcall(void *)
{
	while (1) {
		Lx::scheduler().current()->block_and_schedule();

		_socket->exec_call();
	}
}


/**************************
 ** Socket_call instance **
 **************************/

Wifi::Socket_call socket_call;


/***************************
 ** Socket_call interface **
 ***************************/

using namespace Wifi;


Wifi::Socket *Socket_call::socket(int domain, int type, int protocol)
{
	/* FIXME domain, type, protocol values */
	_call.opcode          = Call::SOCKET;
	_call.socket.domain   = domain;
	_call.socket.type     = type;
	_call.socket.protocol = protocol;

	_socket->submit_and_block();

	if (_call.socket.result == 0)
		return 0;

	Wifi::Socket *s = new (_alloc) Wifi::Socket(_call.socket.result);

	return s;
}


int Socket_call::close(Socket *s)
{
	_call.opcode = Call::CLOSE;
	_call.handle = s;

	_socket->submit_and_block();

	if (_call.err) {
		Genode::error("closing socket failed: ", _call.err);
	}

	destroy(_alloc, s);
	return 0;
}


int Socket_call::bind(Socket *s, Wifi::Sockaddr const *addr, unsigned addrlen)
{
	/* FIXME convert to/from Sockaddr */
	_call.opcode       = Call::BIND;
	_call.handle       = s;
	_call.bind.addr    = (sockaddr const *)addr;
	_call.bind.addrlen = addrlen;

	_socket->submit_and_block();

	return _call.err;
}


int Socket_call::getsockname(Socket *s, Wifi::Sockaddr *addr, unsigned *addrlen)
{
	/* FIXME convert to/from Sockaddr */
	/* FIXME unsigned * -> int * */
	_call.opcode              = Call::GETSOCKNAME;
	_call.handle              = s;
	_call.getsockname.addr    = (sockaddr *)addr;
	_call.getsockname.addrlen = (int *)addrlen;

	_socket->submit_and_block();

	return _call.err;
}


int Socket_call::poll_all(Poll_socket_fd *s, unsigned num, int timeout)
{
	_call.opcode = Call::POLL_ALL;
	_call.handle = 0;
	_call.poll_all.sockets = s;
	_call.poll_all.num     = num;
	_call.poll_all.timeout = timeout;

	_socket->submit_and_block();

	return _call.err;
}


static int msg_flags(Wifi::Flags in)
{
	int out = Wifi::WIFI_F_NONE;
	if (in & Wifi::WIFI_F_MSG_ERRQUEUE)
		out |= MSG_ERRQUEUE;

	return out;
};


Wifi::ssize_t Socket_call::recvmsg(Socket *s, Wifi::Msghdr *msg, Wifi::Flags flags)
{
	_call.opcode                       = Call::RECVMSG;
	_call.handle                       = s;
	_call.recvmsg.msg.msg_name         = msg->msg_name;
	_call.recvmsg.msg.msg_namelen      = msg->msg_namelen;
	_call.recvmsg.msg.msg_iter.iov     = _call.recvmsg.iov;
	_call.recvmsg.msg.msg_iter.nr_segs = msg->msg_iovlen;
	_call.recvmsg.msg.msg_iter.count   = msg->msg_count;
	_call.recvmsg.msg.msg_control      = msg->msg_control;
	_call.recvmsg.msg.msg_controllen   = msg->msg_controllen;
	_call.recvmsg.flags                = msg_flags(flags);

	for (unsigned i = 0; i < msg->msg_iovlen; ++i) {
		_call.recvmsg.iov[i].iov_base = msg->msg_iov[i].iov_base;
		_call.recvmsg.iov[i].iov_len  = msg->msg_iov[i].iov_len;
	}

	_socket->submit_and_block();

	msg->msg_namelen = _call.recvmsg.msg.msg_namelen;

	return _call.err;
}


Wifi::ssize_t Socket_call::sendmsg(Socket *s, Wifi::Msghdr const *msg, Wifi::Flags flags)
{
	_call.opcode                       = Call::SENDMSG;
	_call.handle                       = s;
	_call.sendmsg.msg.msg_name         = msg->msg_name;
	_call.sendmsg.msg.msg_namelen      = msg->msg_namelen;
	_call.sendmsg.msg.msg_iter.iov     = _call.sendmsg.iov;
	_call.sendmsg.msg.msg_iter.nr_segs = msg->msg_iovlen;
	_call.sendmsg.msg.msg_iter.count   = msg->msg_count;
	_call.sendmsg.msg.msg_control      = 0;
	_call.sendmsg.msg.msg_controllen   = 0;
	_call.sendmsg.flags                = msg_flags(flags);

	for (unsigned i = 0; i < msg->msg_iovlen; ++i) {
		_call.sendmsg.iov[i].iov_base = msg->msg_iov[i].iov_base;
		_call.sendmsg.iov[i].iov_len  = msg->msg_iov[i].iov_len;
	}

	_socket->submit_and_block();

	return _call.err;
}


static int sockopt_level(Sockopt_level const in)
{
	switch (in) {
	case Wifi::WIFI_SOL_SOCKET:  return SOL_SOCKET;
	case Wifi::WIFI_SOL_NETLINK: return SOL_NETLINK;
	}

	return -1;
}


static int sockopt_name(Sockopt_level const level, Sockopt_name const in)
{
	switch (level) {
	case Wifi::WIFI_SOL_SOCKET:
		switch (in) {
		case Wifi::WIFI_SO_SNDBUF:      return SO_SNDBUF;
		case Wifi::WIFI_SO_RCVBUF:      return SO_RCVBUF;
		case Wifi::WIFI_SO_PASSCRED:    return SO_PASSCRED;
		case Wifi::WIFI_SO_WIFI_STATUS: return SO_WIFI_STATUS;

		default: return -1;
		}
	case Wifi::WIFI_SOL_NETLINK:
		switch (in) {
		case Wifi::WIFI_NETLINK_ADD_MEMBERSHIP:  return NETLINK_ADD_MEMBERSHIP;
		case Wifi::WIFI_NETLINK_DROP_MEMBERSHIP: return NETLINK_DROP_MEMBERSHIP;
		case Wifi::WIFI_NETLINK_PKTINFO:         return NETLINK_PKTINFO;

		default: return -1;
		}
	}

	return -1;
}


int Socket_call::setsockopt(Socket *s,
                            Wifi::Sockopt_level level, Wifi::Sockopt_name optname,
                            const void *optval, uint32_t optlen)
{
	/* FIXME optval values */
	_call.opcode             = Call::SETSOCKOPT;
	_call.handle             = s;
	_call.setsockopt.level   = sockopt_level(level);
	_call.setsockopt.optname = sockopt_name(level, optname);
	_call.setsockopt.optval  = optval;
	_call.setsockopt.optlen  = optlen;

	_socket->submit_and_block();

	return _call.err;
}


void Socket_call::non_block(Socket *s, bool value)
{
	_call.opcode          = Call::NON_BLOCK;
	_call.handle          = s;
	_call.non_block.value = value;

	_socket->submit_and_block();
}


void Socket_call::get_mac_address(unsigned char *addr)
{
	_call.opcode               = Call::GET_MAC_ADDRESS;
	_call.handle               = 0;
	_call.get_mac_address.addr = addr;

	_socket->submit_and_block();
}
