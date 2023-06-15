	/*
 * \brief  Linux socket call interface front end
 * \author Josef Soentgen
 * \date   2014-08-04
 */

/*
 * Copyright (C) 2014-2022 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

/* Genode includes */
#include <base/allocator.h>
#include <base/env.h>
#include <base/log.h>
#include <net/mac_address.h>

/* DDE Linux includes */
#include <wifi/socket_call.h>
#include <lx_emul/task.h>
#include <lx_kit/env.h>

/* local includes */
#include "lx_user.h"
#include "lx_socket_call.h"
#include "libc_errno.h"


using namespace Genode;


/*
 * The values were taken from 'uapi/asm-generic/socket.h',
 * 'uapi/linux/netlink.h' and 'linux/socket.h' and must be
 * kept in sync.
 */
enum : int {
	SOL_SOCKET  = 1,
	SOL_NETLINK = 270,

	SO_SNDBUF      = 7,
	SO_RCVBUF      = 8,
	SO_PASSCRED    = 16,
	SO_WIFI_STATUS = 41,

	NETLINK_ADD_MEMBERSHIP  = 1,
	NETLINK_DROP_MEMBERSHIP = 2,
	NETLINK_PKTINFO         = 3,

	MSG_DONTWAIT = 0x40,
	MSG_ERRQUEUE = 0x2000,
};


static int convert_errno_from_linux(int linux_errno)
{
	if (linux_errno >= 0)
		return linux_errno;

	linux_errno *= -1;

	switch (linux_errno) {
	case 0:                return 0;
	case E2BIG:            return -(int)Libc::Errno::BSD_E2BIG;
	case EACCES:           return -(int)Libc::Errno::BSD_EACCES;
	case EADDRINUSE:       return -(int)Libc::Errno::BSD_EADDRINUSE;
	case EADDRNOTAVAIL:    return -(int)Libc::Errno::BSD_EADDRNOTAVAIL;
	case EAFNOSUPPORT:     return -(int)Libc::Errno::BSD_EAFNOSUPPORT;
	case EAGAIN:           return -(int)Libc::Errno::BSD_EAGAIN;
	case EALREADY:         return -(int)Libc::Errno::BSD_EALREADY;
	case EBADF:            return -(int)Libc::Errno::BSD_EBADF;
	case EBADMSG:          return -(int)Libc::Errno::BSD_EBADMSG;
	case EBUSY:            return -(int)Libc::Errno::BSD_EBUSY;
	case ECANCELED:        return -(int)Libc::Errno::BSD_ECANCELED;
	case ECONNABORTED:     return -(int)Libc::Errno::BSD_ECONNABORTED;
	case ECONNREFUSED:     return -(int)Libc::Errno::BSD_ECONNREFUSED;
	case ECONNRESET:       return -(int)Libc::Errno::BSD_ECONNRESET;
	case EDEADLK:          return -(int)Libc::Errno::BSD_EDEADLK;
	case EDESTADDRREQ:     return -(int)Libc::Errno::BSD_EDESTADDRREQ;
	case EDOM:             return -(int)Libc::Errno::BSD_EDOM;
	case EEXIST:           return -(int)Libc::Errno::BSD_EEXIST;
	case EFAULT:           return -(int)Libc::Errno::BSD_EFAULT;
	case EFBIG:            return -(int)Libc::Errno::BSD_EFBIG;
	case EHOSTDOWN:        return -(int)Libc::Errno::BSD_EHOSTDOWN;
	case EHOSTUNREACH:     return -(int)Libc::Errno::BSD_EHOSTUNREACH;
	case EILSEQ:           return -(int)Libc::Errno::BSD_EILSEQ;
	case EINPROGRESS:      return -(int)Libc::Errno::BSD_EINPROGRESS;
	case EINTR:            return -(int)Libc::Errno::BSD_EINTR;
	case EINVAL:           return -(int)Libc::Errno::BSD_EINVAL;
	case EIO:              return -(int)Libc::Errno::BSD_EIO;
	case EISCONN:          return -(int)Libc::Errno::BSD_EISCONN;
	case EMSGSIZE:         return -(int)Libc::Errno::BSD_EMSGSIZE;
	case ENAMETOOLONG:     return -(int)Libc::Errno::BSD_ENAMETOOLONG;
	case ENETDOWN:         return -(int)Libc::Errno::BSD_ENETDOWN;
	case ENETUNREACH:      return -(int)Libc::Errno::BSD_ENETUNREACH;
	case ENFILE:           return -(int)Libc::Errno::BSD_ENFILE;
	case ENOBUFS:          return -(int)Libc::Errno::BSD_ENOBUFS;
	case ENODEV:           return -(int)Libc::Errno::BSD_ENODEV;
	case ENOENT:           return -(int)Libc::Errno::BSD_ENOENT;
	case ENOEXEC:          return -(int)Libc::Errno::BSD_ENOEXEC;
	case ENOLINK:
	                       error("ENOLINK (", (int) ENOLINK, ") -> ", (int)Libc::Errno::BSD_ENOLINK);
	                       return -(int)Libc::Errno::BSD_ENOLINK;
	case ENOMEM:           return -(int)Libc::Errno::BSD_ENOMEM;
	case ENOMSG:           return -(int)Libc::Errno::BSD_ENOMSG;
	case ENOPROTOOPT:      return -(int)Libc::Errno::BSD_ENOPROTOOPT;
	case ENOSPC:           return -(int)Libc::Errno::BSD_ENOSPC;
	case ENOSYS:           return -(int)Libc::Errno::BSD_ENOSYS;
	case ENOTCONN:         return -(int)Libc::Errno::BSD_ENOTCONN;
	case ENOTSOCK:         return -(int)Libc::Errno::BSD_ENOTSOCK;
	case ENOTTY:           return -(int)Libc::Errno::BSD_ENOTTY;
	case ENXIO:            return -(int)Libc::Errno::BSD_ENXIO;
	case EOPNOTSUPP:       return -(int)Libc::Errno::BSD_EOPNOTSUPP;
	case EOVERFLOW:        return -(int)Libc::Errno::BSD_EOVERFLOW;
	case EPERM:            return -(int)Libc::Errno::BSD_EPERM;
	case EPFNOSUPPORT:     return -(int)Libc::Errno::BSD_EPFNOSUPPORT;
	case EPIPE:            return -(int)Libc::Errno::BSD_EPIPE;
	case EPROTO:           return -(int)Libc::Errno::BSD_EPROTO;
	case EPROTONOSUPPORT:  return -(int)Libc::Errno::BSD_EPROTONOSUPPORT;
	case ERANGE:           return -(int)Libc::Errno::BSD_ERANGE;
	case ESOCKTNOSUPPORT:  return -(int)Libc::Errno::BSD_ESOCKTNOSUPPORT;
	case ESPIPE:           return -(int)Libc::Errno::BSD_ESPIPE;
	case ESRCH:            return -(int)Libc::Errno::BSD_ESRCH;
	case ETIMEDOUT:        return -(int)Libc::Errno::BSD_ETIMEDOUT;
	case EXDEV:            return -(int)Libc::Errno::BSD_EXDEV;
	default:
		error(__func__, ": unhandled errno ", linux_errno);
		return linux_errno;
	}
}


static_assert((unsigned)Wifi::Msghdr::MAX_IOV_LEN == (unsigned)MAX_IOV_LEN);



/* XXX move Wifi::Socket definition to better location */
struct Wifi::Socket
{
	void *socket    = nullptr;
	bool  non_block = false;

	Socket() { }

	explicit Socket(void *s) : socket(s) { }

	void print(Output &out) const
	{
		Genode::print(out, "this: ", this, " socket: ", socket, " non_block: ", non_block);
	}
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
			void const *addr;
			int         addrlen;
		} bind;
		struct
		{
			void *addr;
			int  *addrlen;
		} getsockname;
		struct
		{
			lx_msghdr msg;
			int    flags;
		} recvmsg;
		struct
		{
			lx_msghdr msg;
			int    flags;
		} sendmsg;
		struct {
			int         level;
			int         optname;
			void const *optval;
			unsigned    optlen;
		} setsockopt;
		struct
		{
			unsigned char *addr;
			unsigned int   addr_len;
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

static Call      _call;
static Semaphore _block;


namespace Lx {
	class Socket;
}



/**
 * Context for socket calls
 */
class Lx::Socket
{
	private:

		Socket(const Socket&) = delete;
		Socket& operator=(const Socket&) = delete;

		Signal_transmitter         _sender { };
		Signal_handler<Lx::Socket> _dispatcher;

		Signal_handler<Lx::Socket> _dispatcher_blockade;

		struct socket *_sock_poll_table[Wifi::MAX_POLL_SOCKETS] { };

		struct socket *_call_socket()
		{
			struct socket *sock = static_cast<struct socket*>(_call.handle->socket);
			if (!sock)
				error("BUG: sock is zero");

			return sock;
		}

		void _do_socket()
		{
			struct socket *s;
			int res = lx_sock_create_kern(_call.socket.domain, _call.socket.type,
			                              _call.socket.protocol, &s);
			if (!res) {
				_call.socket.result = s;
				_call.err           = 0;
				return;
			}

			_call.socket.result = nullptr;
			_call.err           = res;
		}

		void _do_close()
		{
			struct socket *sock = _call_socket();

			_call.err = lx_sock_release(sock);
		}

		void _do_bind()
		{
			struct socket *sock = _call_socket();

			_call.err = lx_sock_bind(sock,
			                         const_cast<void*>(_call.bind.addr),
			                         _call.bind.addrlen);
		}

		void _do_getsockname()
		{
			struct socket *sock    = _call_socket();
			int            addrlen = *_call.getsockname.addrlen;

			_call.err = lx_sock_getname(sock, _call.getsockname.addr, 0);

			*_call.getsockname.addrlen = addrlen;
		}

		void _do_recvmsg()
		{
			struct socket *sock = _call_socket();

			_call.err = lx_sock_recvmsg(sock, &_call.recvmsg.msg,
			                            _call.recvmsg.flags,
			                            _call.handle->non_block);
		}

		void _do_sendmsg()
		{
			struct socket *sock = _call_socket();

			_call.err = lx_sock_sendmsg(sock, &_call.sendmsg.msg,
			                            _call.sendmsg.flags,
			                            _call.handle->non_block);
		}

		void _do_setsockopt()
		{
			struct socket *sock = _call_socket();

			_call.err = lx_sock_setsockopt(sock,
				                           _call.setsockopt.level,
				                           _call.setsockopt.optname,
				                           _call.setsockopt.optval,
				                           _call.setsockopt.optlen);
		}

		void _do_get_mac_address()
		{
			unsigned const char *addr = lx_get_mac_addr();
			if (!addr)
				return;

			size_t const copy = 6 > _call.get_mac_address.addr_len
			                  ? _call.get_mac_address.addr_len
			                  : 6;

			memcpy(_call.get_mac_address.addr, addr, copy);
		}

		void _do_poll_all()
		{
			Wifi::Poll_socket_fd *sockets = _call.poll_all.sockets;
			unsigned num                  = _call.poll_all.num;
			int timeout                   = _call.poll_all.timeout;

			int nready             = 0;
			bool timeout_triggered = false;
			bool woken_up          = false;
			do {
				/**
				 * Timeout was triggered, exit early.
				 */
				if (timeout_triggered) {
					break;
				}

				/**
				 * Poll each socket and check if there is something of interest.
				 */
				for (unsigned i = 0; i < num; i++) {
					struct socket *sock = static_cast<struct socket*>(sockets[i].s->socket);

					struct lx_poll_result result = lx_sock_poll(sock);

					sockets[i].revents = 0;
					if (result.in)
						sockets[i].revents |= sockets[i].events & Wifi::WIFI_POLLIN ? Wifi::WIFI_POLLIN : 0;
					if (result.out)
						sockets[i].revents |= sockets[i].events & Wifi::WIFI_POLLOUT ? Wifi::WIFI_POLLOUT : 0;
					if (result.ex)
						sockets[i].revents |= sockets[i].events & Wifi::WIFI_POLLEX ? Wifi::WIFI_POLLEX : 0;

					if (sockets[i].revents)
						nready++;
				}

				/**
				 * We were woken up but there is still nothing of interest.
				 */
				if (woken_up) {
					break;
				}

				/**
				 * Exit the loop if either a socket is ready or there is
				 * no timeout given.
				 */
				if (nready || !timeout) {
					break;
				}

				/**
				 * In case of a timeout add all sockets to an artificial wait list
				 * so at least one is woken up an sk_data_ready() call.
				 */
				for (unsigned i = 0; i < num; i++) {
					struct socket *sock = static_cast<struct socket*>(sockets[i].s->socket);
					_sock_poll_table[i] = sock;
				}

				timeout_triggered = !lx_sock_poll_wait(_sock_poll_table, num, timeout);

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
			lx_emul_task_unblock(socketcall_task_struct_ptr);
			Lx_kit::env().scheduler.execute();
		}

		void _handle_blockade()
		{
			_block.up();
		}

	public:

		Socket(Entrypoint &ep)
		:
			_dispatcher(ep, *this, &Lx::Socket::_handle),
			_dispatcher_blockade(ep, *this, &Lx::Socket::_handle_blockade)
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

			case Call::NONE: [[fallthrough]];/* ignore silently */
			default:
				break;
			}

			/*
			 * Save old call opcode as we may only release the blocker
			 * when actually did something useful, i.e., were called by
			 * some socket operation and not by kicking the socket.
			 */
			Call::Opcode old = _call.opcode;

			_call.opcode = Call::NONE;

			if (old != Call::NONE) { _dispatcher_blockade.local_submit(); }
		}

		void submit_and_block()
		{
			_sender.submit();
			_block.down();
		}
};


static Lx::Socket *_socket;

/* implemented in wlan.cc */
void _wifi_report_mac_address(Net::Mac_address const &mac_address);


extern "C" int socketcall_task_function(void *)
{
	static Lx::Socket inst(Lx_kit::env().env.ep());
	_socket = &inst;

	void const *mac_addr = nullptr;

	while (true) {

		/*
		 * Try to report the MAC address once. We have to check
		 * 'lx_get_mac_addr' as it might by NULL in case 'wlan0'
		 * is not yet available.
		 */
		if (!mac_addr) {
			mac_addr = lx_get_mac_addr();
			if (mac_addr)
				_wifi_report_mac_address({ (void *) mac_addr });
		}

		_socket->exec_call();

		lx_emul_task_schedule(true);
	}
}


void wifi_kick_socketcall()
{
	/* ignore silently, the function might be called to before init */
	if (!_socket) { return; }

	lx_emul_task_unblock(socketcall_task_struct_ptr);
	Lx_kit::env().scheduler.execute();
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
	_call.socket.type     = type & 0xff;
	_call.socket.protocol = protocol;

	_socket->submit_and_block();

	if (_call.socket.result == 0)
		return 0;

	Wifi::Socket *s = new (Lx_kit::env().heap) Wifi::Socket(_call.socket.result);

	return s;
}


int Socket_call::close(Socket *s)
{
	_call.opcode = Call::CLOSE;
	_call.handle = s;

	_socket->submit_and_block();

	if (_call.err)
		warning("closing socket failed: ", _call.err);

	destroy(Lx_kit::env().heap, s);
	return _call.err;
}


int Socket_call::bind(Socket *s, Wifi::Sockaddr const *addr, unsigned addrlen)
{
	/* FIXME convert to/from Sockaddr */
	_call.opcode       = Call::BIND;
	_call.handle       = s;
	_call.bind.addr    = (void const *)addr;
	_call.bind.addrlen = addrlen;

	_socket->submit_and_block();

	return convert_errno_from_linux(_call.err);
}


int Socket_call::getsockname(Socket *s, Wifi::Sockaddr *addr, unsigned *addrlen)
{
	/* FIXME convert to/from Sockaddr */
	/* FIXME unsigned * -> int * */
	_call.opcode              = Call::GETSOCKNAME;
	_call.handle              = s;
	_call.getsockname.addr    = (void *)addr;
	_call.getsockname.addrlen = (int *)addrlen;

	_socket->submit_and_block();

	return convert_errno_from_linux(_call.err);
}


int Socket_call::poll_all(Poll_socket_fd *s, unsigned num, int timeout)
{
	_call.opcode = Call::POLL_ALL;
	_call.handle = 0;
	_call.poll_all.sockets = s;
	_call.poll_all.num     = num;
	_call.poll_all.timeout = timeout;

	_socket->submit_and_block();

	return convert_errno_from_linux(_call.err);
}


static inline int msg_flags(Wifi::Flags in)
{
	int out = Wifi::WIFI_F_NONE;
	if (in & Wifi::WIFI_F_MSG_ERRQUEUE)
		out |= MSG_ERRQUEUE;
	if (in & Wifi::WIFI_F_MSG_DONTWAIT) {
		out |= MSG_DONTWAIT;
	}

	return out;
};


Wifi::ssize_t Socket_call::recvmsg(Socket *s, Wifi::Msghdr *msg, Wifi::Flags flags)
{
	_call.opcode                       = Call::RECVMSG;
	_call.handle                       = s;
	_call.recvmsg.msg.msg_name         = msg->msg_name;
	_call.recvmsg.msg.msg_namelen      = msg->msg_namelen;
	_call.recvmsg.msg.msg_iovcount     = msg->msg_iovlen;

	_call.recvmsg.msg.msg_control      = msg->msg_control;
	_call.recvmsg.msg.msg_controllen   = msg->msg_controllen;
	_call.recvmsg.flags                = msg_flags(flags);

	for (unsigned i = 0; i < msg->msg_iovlen; ++i) {
		_call.recvmsg.msg.msg_iov[i].iov_base = msg->msg_iov[i].iov_base;
		_call.recvmsg.msg.msg_iov[i].iov_len  = msg->msg_iov[i].iov_len;
	}

	_socket->submit_and_block();

	msg->msg_namelen = _call.recvmsg.msg.msg_namelen;

	return convert_errno_from_linux(_call.err);
}


Wifi::ssize_t Socket_call::sendmsg(Socket *s, Wifi::Msghdr const *msg, Wifi::Flags flags)
{
	_call.opcode                       = Call::SENDMSG;
	_call.handle                       = s;
	_call.sendmsg.msg.msg_name         = msg->msg_name;
	_call.sendmsg.msg.msg_namelen      = msg->msg_namelen;
	_call.sendmsg.msg.msg_iovcount     = msg->msg_iovlen;

	_call.sendmsg.msg.msg_control      = 0;
	_call.sendmsg.msg.msg_controllen   = 0;
	_call.sendmsg.flags                = msg_flags(flags);

	for (unsigned i = 0; i < msg->msg_iovlen; ++i) {
		_call.sendmsg.msg.msg_iov[i].iov_base = msg->msg_iov[i].iov_base;
		_call.sendmsg.msg.msg_iov[i].iov_len  = msg->msg_iov[i].iov_len;
	}

	_socket->submit_and_block();

	return convert_errno_from_linux(_call.err);
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
                            const void *optval, unsigned optlen)
{
	/* FIXME optval values */
	_call.opcode             = Call::SETSOCKOPT;
	_call.handle             = s;
	_call.setsockopt.level   = sockopt_level(level);
	_call.setsockopt.optname = sockopt_name(level, optname);
	_call.setsockopt.optval  = optval;
	_call.setsockopt.optlen  = optlen;

	_socket->submit_and_block();

	return convert_errno_from_linux(_call.err);
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
	_call.get_mac_address.addr     = addr;
	_call.get_mac_address.addr_len = 6; // XXX enforce and set from caller

	_socket->submit_and_block();
}
