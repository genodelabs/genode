/*
 * \brief  C-interface to Linux socked kernel code
 * \author Sebastian Sumpf
 * \date   2024-01-29
 *
 * Can only be called by lx_lit tasks
 */

/*
 * Copyright (C) 2024 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

#include <linux/net.h>
#include <net/sock.h>

#include "lx_socket.h"

extern struct net init_net;


static enum Errno _genode_errno(int errno)
{
	if (errno < 0) errno *= -1;

	switch (errno) {
	case 0:               return GENODE_ENONE;
	case E2BIG:           return GENODE_E2BIG;
	case EACCES:          return GENODE_EACCES;
	case EADDRINUSE:      return GENODE_EADDRINUSE;
	case EADDRNOTAVAIL:   return GENODE_EADDRNOTAVAIL;
	case EAFNOSUPPORT:    return GENODE_EAFNOSUPPORT;
	case EAGAIN:          return GENODE_EAGAIN;
	case EALREADY:        return GENODE_EALREADY;
	case EBADF:           return GENODE_EBADF;
	case EBADFD:          return GENODE_EBADFD;
	case EBADMSG:         return GENODE_EBADMSG;
	case EBADRQC:         return GENODE_EBADRQC;
	case EBUSY:           return GENODE_EBUSY;
	case ECONNABORTED:    return GENODE_ECONNABORTED;
	case ECONNREFUSED:    return GENODE_ECONNREFUSED;
	case EDESTADDRREQ:    return GENODE_EDESTADDRREQ;
	case EDOM:            return GENODE_EDOM;
	case EEXIST:          return GENODE_EEXIST;
	case EFAULT:          return GENODE_EFAULT;
	case EFBIG:           return GENODE_EFBIG;
	case EHOSTUNREACH:    return GENODE_EHOSTUNREACH;
	case EINPROGRESS:     return GENODE_EINPROGRESS;
	case EINTR:           return GENODE_EINTR;
	case EINVAL:          return GENODE_EINVAL;
	case EIO:             return GENODE_EIO;
	case EISCONN:         return GENODE_EISCONN;
	case ELOOP:           return GENODE_ELOOP;
	case EMLINK:          return GENODE_EMLINK;
	case EMSGSIZE:        return GENODE_EMSGSIZE;
	case ENAMETOOLONG:    return GENODE_ENAMETOOLONG;
	case ENETDOWN:        return GENODE_ENETDOWN;
	case ENETUNREACH:     return GENODE_ENETUNREACH;
	case ENFILE:          return GENODE_ENFILE;
	case ENOBUFS:         return GENODE_ENOBUFS;
	case ENODATA:         return GENODE_ENODATA;
	case ENODEV:          return GENODE_ENODEV;
	case ENOENT:          return GENODE_ENOENT;
	case ENOIOCTLCMD:     return GENODE_ENOIOCTLCMD;
	case ENOLINK:         return GENODE_ENOLINK;
	case ENOMEM:          return GENODE_ENOMEM;
	case ENOMSG:          return GENODE_ENOMSG;
	case ENOPROTOOPT:     return GENODE_ENOPROTOOPT;
	case ENOSPC:          return GENODE_ENOSPC;
	case ENOSYS:          return GENODE_ENOSYS;
	case ENOTCONN:        return GENODE_ENOTCONN;
	case ENOTSUPP:        return GENODE_ENOTSUPP;
	case ENOTTY:          return GENODE_ENOTTY;
	case ENXIO:           return GENODE_ENXIO;
	case EOPNOTSUPP:      return GENODE_EOPNOTSUPP;
	case EOVERFLOW:       return GENODE_EOVERFLOW;
	case EPERM:           return GENODE_EPERM;
	case EPFNOSUPPORT:    return GENODE_EPFNOSUPPORT;
	case EPIPE:           return GENODE_EPIPE;
	case EPROTO:          return GENODE_EPROTO;
	case EPROTONOSUPPORT: return GENODE_EPROTONOSUPPORT;
	case EPROTOTYPE:      return GENODE_EPROTOTYPE;
	case ERANGE:          return GENODE_ERANGE;
	case EREMCHG:         return GENODE_EREMCHG;
	case ESOCKTNOSUPPORT: return GENODE_ESOCKTNOSUPPORT;
	case ESPIPE:          return GENODE_ESPIPE;
	case ESRCH:           return GENODE_ESRCH;
	case ESTALE:          return GENODE_ESTALE;
	case ETIMEDOUT:       return GENODE_ETIMEDOUT;
	case ETOOMANYREFS:    return GENODE_ETOOMANYREFS;
	case EUSERS:          return GENODE_EUSERS;
	case EXDEV:           return GENODE_EXDEV;
	case ECONNRESET:      return GENODE_ECONNRESET;
	default:
		printk("%s:%d unsupported errno %d\n",
		       __func__, __LINE__, errno);
	}
	return GENODE_EINVAL;
}


/* index must match with socket.h Genode 'Sock_opt' */
static int sock_opts[] = {
	0,
	SO_DEBUG,
	SO_ACCEPTCONN,
	SO_DONTROUTE,
	SO_LINGER,
	SO_OOBINLINE,
	SO_REUSEPORT,
	SO_SNDBUF,
	SO_RCVBUF,
	SO_SNDLOWAT,
	SO_RCVLOWAT,
	SO_SNDTIMEO_NEW,
	SO_RCVTIMEO_NEW,
	SO_ERROR,
	SO_TYPE,
	SO_BINDTODEVICE,
	SO_BROADCAST,
};


static int _linux_sockopt(enum Sock_opt sockopt)
{
	return sock_opts[sockopt];
}


static struct sockaddr _sockaddr(struct genode_sockaddr const *addr)
{
	struct sockaddr sock_addr = { };

	if (addr->family == AF_UNSPEC) {
		sock_addr.sa_family = AF_UNSPEC;
	}
	else if (addr->family == AF_INET) {
		struct sockaddr_in in_addr = {
			.sin_family      = AF_INET,
			.sin_port        = addr->in.port,
			.sin_addr.s_addr = addr->in.addr
		};
		memcpy(&sock_addr, &in_addr, sizeof(in_addr));
	} else {
		printk("%s:%d error: family %d not implemented\n", __func__, __LINE__,
		       addr->family);
	}
	return sock_addr;
}


static void _genode_sockaddr(struct genode_sockaddr *addr,
                             struct sockaddr const  *linux_addr,
                             int length)
{
	if (length == sizeof(struct sockaddr_in)) {
		struct sockaddr_in const * in = (struct sockaddr_in const *)linux_addr;
		addr->family             = in->sin_family;
		addr->in.port            = in->sin_port;
		addr->in.addr            = in->sin_addr.s_addr;
	} else
		printk("%s:%d: unknown sockaddr length %d\n", __func__, __LINE__, length);
}


static int _sockaddr_len(struct genode_sockaddr const *addr)
{
	if (addr->family == AF_INET)
		return sizeof(struct sockaddr_in);

	printk("error: _sockaddr_len unknown family: %u\n", addr->family);
	return 0;
}


struct socket *lx_sock_alloc(void)                  { return sock_alloc(); }
void           lx_sock_release(struct socket* sock) { sock_release(sock);  }


extern int __setup_ip_auto_config_setup(char *);

void lx_socket_address(struct genode_socket_config *config)
{
	if (config->dhcp) {
		__setup_ip_auto_config_setup("dhcp");
	}
	else {
		char address_config[128];
		snprintf(address_config, sizeof(address_config),
		         "%s::%s:%s:::off:%s",
		         config->ip_addr, config->gateway, config->netmask,
		         config->nameserver);
		__setup_ip_auto_config_setup(address_config);
	}
	lx_emul_initcall("__initcall_ip_auto_config7");
};


void lx_socket_mtu(unsigned mtu)
{
	/* zero mtu means reset to default */
	unsigned new_mtu = mtu ? mtu : ETH_DATA_LEN;

	struct net        *net;
	struct net_device *dev;

	for_each_net(net) {
		for_each_netdev(net, dev) {
			dev_set_mtu(dev, new_mtu);
		}
	}
}


enum Errno lx_socket_create(int domain, int type, int protocol,
                            struct socket **res)
{
	int err = sock_create_kern(&init_net, domain, type, protocol, res);
	if (err) return _genode_errno(err);

	init_waitqueue_head(&(*res)->wq.wait);

	return GENODE_ENONE;
}


enum Errno lx_socket_bind(struct socket *sock, struct genode_sockaddr const *addr)
{
	struct sockaddr sock_addr = _sockaddr(addr);
	return _genode_errno(sock->ops->bind(sock, &sock_addr, _sockaddr_len(addr)));
}


enum Errno lx_socket_listen(struct socket *sock, int length)
{
	return _genode_errno(sock->ops->listen(sock, length));
}


enum Errno lx_socket_accept(struct socket *sock, struct socket *new_sock,
                            struct genode_sockaddr *addr)
{
	struct sockaddr linux_addr;
	int err;

	new_sock->type = sock->type;
	new_sock->ops  = sock->ops;

	err = sock->ops->accept(sock, new_sock, O_NONBLOCK, true);

	if (err == 0) {
		err = sock->ops->getname(new_sock, &linux_addr, 0);
		if (err > 0) _genode_sockaddr(addr, &linux_addr, err);
	}

	return err < 0 ? _genode_errno(err) : GENODE_ENONE;
}


enum Errno lx_socket_connect(struct socket *sock, struct genode_sockaddr const *addr)
{
	struct sockaddr sock_addr = _sockaddr(addr);
	return _genode_errno(sock->ops->connect(sock, &sock_addr, _sockaddr_len(addr),
	                     O_NONBLOCK));
}


unsigned lx_socket_pollin_set(void)
{
	return (POLLRDNORM | POLLRDBAND | POLLIN | POLLHUP | POLLERR);
}


unsigned lx_socket_pollout_set(void)
{
	return (POLLWRBAND | POLLWRNORM | POLLOUT | POLLERR);
}


unsigned lx_socket_pollex_set(void)
{
	return POLLPRI;
}


unsigned lx_socket_poll(struct socket *sock)
{
	struct file file = { };
	return sock->ops->poll(&file, sock, NULL);
}


enum Errno lx_socket_getsockopt(struct socket *sock, enum Sock_level level,
                                enum Sock_opt opt, void *optval, unsigned *optlen)
{
	int name = _linux_sockopt(opt);
	enum Errno errno;
	int err;

	if (level != GENODE_SOL_SOCKET) return GENODE_ENOPROTOOPT;

	if (opt == GENODE_SO_ERROR && *optlen < sizeof(enum Sock_opt))
		return GENODE_EFAULT;

	if (level == GENODE_SOL_SOCKET)
		err = sock_getsockopt(sock, SOL_SOCKET, name, optval, optlen);
	/* we might need this later
	else {
		err = sock->ops->getsockopt(sock, SOL_SOCKET, name, optval, optlen);
	}
	*/

	if (err) return _genode_errno(err);

	if (opt == GENODE_SO_ERROR) {
		err   = *((int *)optval);
		errno = _genode_errno(err);
		memcpy(optval, &errno, sizeof(enum Sock_opt));
	}

	return GENODE_ENONE;
}


enum Errno lx_socket_setsockopt(struct socket *sock, enum Sock_level level,
                                enum Sock_opt opt, void const *optval, unsigned optlen)
{
	int name = _linux_sockopt(opt);
	int err;

	if (level != GENODE_SOL_SOCKET) return GENODE_ENOPROTOOPT;

	if (level == GENODE_SOL_SOCKET) {
		sockptr_t val = { .user = optval, .is_kernel = 0 };
		err = sock_setsockopt(sock, SOL_SOCKET, name, val, optlen);
	}
	/* we might need this later
	else {
		err = sock->ops->getsockopt(sock, SOL_SOCKET, name, optval, optlen);
	}
	*/

	if (err) return _genode_errno(err);

	return GENODE_ENONE;
}


enum Errno lx_socket_getname(struct socket *sock, struct genode_sockaddr *addr, bool peer)
{
	struct sockaddr linux_addr;

	int err = sock->ops->getname(sock, &linux_addr, peer ? 1 : 0);

	if (err < 0) return _genode_errno(err);

	_genode_sockaddr(addr, &linux_addr, err);

	return GENODE_ENONE;
}


static struct msghdr *_create_msghdr(struct genode_msghdr *msg, bool write)
{
	struct msghdr *msghdr;
	struct sockaddr_storage *storage = NULL;
	unsigned long total = 0;

	msghdr = (struct msghdr *)kzalloc(sizeof(*msghdr), GFP_KERNEL);
	if (!msghdr) goto msghdr;

	/* sockaddr */
	if (msg->name) {
		struct sockaddr sock_addr = _sockaddr(msg->name);

		storage = (struct sockaddr_storage *)kmalloc(sizeof(*storage), GFP_KERNEL);
		if (!storage) goto sock_addr;
		memcpy(storage, &sock_addr, _sockaddr_len(msg->name));

		msghdr->msg_name    = storage;
		msghdr->msg_namelen = _sockaddr_len(msg->name);
	}

	/* iovec iterator */
	msghdr->msg_iter.iter_type   = ITER_IOVEC;
	msghdr->msg_iter.data_source = !write;
	msghdr->msg_iter.nr_segs     = msg->iovlen;
	msghdr->msg_iter.__iov       = (struct iovec *)msg->iov;

	for (unsigned i = 0; i < msg->iovlen; i++)
		total += msg->iov[i].size;

	msghdr->msg_iter.count = total;

	/* non-blocking */
	msghdr->msg_flags = MSG_DONTWAIT;

	return msghdr;

sock_addr:
	kfree(msghdr);
msghdr:

	return NULL;
}


static void _destroy_msghdr(struct msghdr *msg)
{
	if (msg->msg_name) kfree(msg->msg_name);
	kfree(msg);
}


enum Errno lx_socket_sendmsg(struct socket *sock, struct genode_msghdr *msg,
                             unsigned long *bytes_send)
{
	struct msghdr *m = _create_msghdr(msg, false);
	int ret;

	if (!m) return GENODE_ENOMEM;

	ret = sock->ops->sendmsg(sock, m, m->msg_iter.count);

	_destroy_msghdr(m);

	if (ret < 0)
		return _genode_errno(ret);

	*bytes_send = (unsigned long)ret;

	return GENODE_ENONE;
}


enum Errno lx_socket_recvmsg(struct socket *sock, struct genode_msghdr *msg,
                             unsigned long *bytes_recv, bool peek)
{
	struct msghdr *m = _create_msghdr(msg, true);
	int ret;
	int flags = MSG_DONTWAIT;

	if (peek) flags |= MSG_PEEK;
	if (!m)   return GENODE_ENOMEM;

	ret = sock->ops->recvmsg(sock, m, m->msg_iter.count, flags);

	/* convert to genode_sockaddr */
	if (ret && msg->name) {
		_genode_sockaddr(msg->name, m->msg_name, _sockaddr_len(msg->name));
	}

	_destroy_msghdr(m);

	if (ret < 0)
		return _genode_errno(ret);

	*bytes_recv = (unsigned long)ret;

	return GENODE_ENONE;
}


enum Errno lx_socket_shutdown(struct socket *sock, int how)
{
	return _genode_errno(sock->ops->shutdown(sock, how));
}


enum Errno lx_socket_release(struct socket *sock)
{
	return _genode_errno(sock->ops->release(sock));
}
