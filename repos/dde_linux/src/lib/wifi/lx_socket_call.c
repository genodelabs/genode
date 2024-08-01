/*
 * \brief  Linux socket call interface back end
 * \author Josef Soentgen
 * \date   2022-02-17
 */

/*
 * Copyright (C) 2022 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

/* DDE Linux includes */
#include "lx_socket_call.h"
#include "lx_user.h"

/* kernel includes */
#include <linux/socket.h>
#include <linux/net.h>
#include <net/sock.h>

struct task_struct *socketcall_task_struct_ptr;
extern int socketcall_task_function(void *p);

extern struct net init_net;


void socketcall_init(void)
{
	int pid =
#if LINUX_VERSION_CODE < KERNEL_VERSION(6,3,0)
		kernel_thread(socketcall_task_function, NULL,
		              CLONE_FS | CLONE_FILES);
#else
		kernel_thread(socketcall_task_function, NULL,
		              "sockcall_task",
		              CLONE_FS | CLONE_FILES);
#endif
	socketcall_task_struct_ptr = find_task_by_pid_ns(pid, NULL);
}


int lx_sock_create_kern(int domain, int type, int protocol,
                        struct socket **res)
{
	int const err = __sock_create(&init_net, domain, type, protocol, res, 1);
	if (err)
		return err;

	init_waitqueue_head(&(*res)->wq.wait);
	return 0;
}


int lx_sock_release(struct socket *sock)
{
	return sock->ops->release(sock);
}


int lx_sock_bind(struct socket *sock, void *sockaddr, int sockaddr_len)
{
	return sock->ops->bind(sock, sockaddr, sockaddr_len);
}


int lx_sock_getname(struct socket *sock, void *sockaddr, int peer)
{
	return sock->ops->getname(sock, sockaddr, peer);
}


int lx_sock_recvmsg(struct socket *sock, struct lx_msghdr *lx_msg,
                    int flags, int dontwait)
{
	struct msghdr *msg;
	struct iovec  *iov;
	size_t         iovlen;
	unsigned       iov_count;
	unsigned       i;

	int err = -1;

	iov_count = lx_msg->msg_iovcount;

	msg = kzalloc(sizeof (struct msghdr), GFP_KERNEL);
	if (!msg)
		goto err_msg;
	iov = kzalloc(sizeof (struct iovec) * iov_count, GFP_KERNEL);
	if (!iov)
		goto err_iov;

	iovlen = 0;
	for (i = 0; i < iov_count; i++) {
		iov[i].iov_base = lx_msg->msg_iov[i].iov_base;
		iov[i].iov_len  = lx_msg->msg_iov[i].iov_len;

		iovlen += lx_msg->msg_iov[i].iov_len;
	}

	msg->msg_name         = lx_msg->msg_name;
	msg->msg_namelen      = lx_msg->msg_namelen;
#if LINUX_VERSION_CODE < KERNEL_VERSION(6,4,0)
	msg->msg_iter.iov     = iov;
#else
	msg->msg_iter.__iov   = iov;
#endif
	msg->msg_iter.nr_segs = iov_count;
	msg->msg_iter.count   = iovlen;

	msg->msg_flags = flags;
	if (dontwait) {
		msg->msg_flags |= MSG_DONTWAIT;
		flags          |= MSG_DONTWAIT;
	}

	err = sock->ops->recvmsg(sock, msg, iovlen, flags);

	kfree(iov);
err_iov:
	kfree(msg);
err_msg:
	return err;
}


int lx_sock_sendmsg(struct socket *sock, struct lx_msghdr* lx_msg,
                    int flags, int dontwait)
{
	struct msghdr *msg;
	struct iovec  *iov;
	size_t         iovlen;
	unsigned       iov_count;
	unsigned       i;

	int err = -1;

	iov_count = lx_msg->msg_iovcount;

	msg = kzalloc(sizeof (struct msghdr), GFP_KERNEL);
	if (!msg)
		goto err_msg;
	iov = kzalloc(sizeof (struct iovec) * iov_count, GFP_KERNEL);
	if (!iov)
		goto err_iov;

	iovlen = 0;
	for (i = 0; i < iov_count; i++) {
		iov[i].iov_base = lx_msg->msg_iov[i].iov_base;
		iov[i].iov_len  = lx_msg->msg_iov[i].iov_len;

		iovlen += lx_msg->msg_iov[i].iov_len;
	}

	msg->msg_name         = lx_msg->msg_name;
	msg->msg_namelen      = lx_msg->msg_namelen;
#if LINUX_VERSION_CODE < KERNEL_VERSION(6,4,0)
	msg->msg_iter.iov     = iov;
#else
	msg->msg_iter.__iov   = iov;
#endif
	msg->msg_iter.nr_segs = iov_count;
	msg->msg_iter.count   = iovlen;

	msg->msg_flags = flags;
	if (dontwait)
		msg->msg_flags |= MSG_DONTWAIT;

	err = sock->ops->sendmsg(sock, msg, iovlen);

	kfree(iov);
err_iov:
	kfree(msg);
err_msg:
	return err;
}


int lx_sock_setsockopt(struct socket *sock, int level, int optname,
                       void const *optval, unsigned optlen)
{
	sockptr_t soptval = (sockptr_t) { .user = optval };

	if (level == SOL_SOCKET)
		return sock_setsockopt(sock, level, optname,
		                       soptval, optlen);

	return sock->ops->setsockopt(sock, level, optname,
	                             soptval, optlen);
}


unsigned char const* lx_get_mac_addr()
{
	static char mac_addr_buffer[16];

	struct sockaddr addr;
	int err;

	memset(mac_addr_buffer, 0, sizeof(mac_addr_buffer));
	memset(&addr, 0, sizeof(addr));

	err = dev_get_mac_address(&addr, &init_net, "wlan0");
	if (err)
		return NULL;

	/*
 	 * The 'struct sockaddr' sa_data union is at least 14 bytes
	 * and we at most copy 6.
	 */
	memcpy(mac_addr_buffer, addr.sa_data, 6);

	return mac_addr_buffer;
}


struct lx_poll_result lx_sock_poll(struct socket *sock)
{
	enum {
		POLLIN_SET  = (EPOLLRDHUP | EPOLLIN | EPOLLRDNORM),
		POLLOUT_SET = (EPOLLOUT | EPOLLWRNORM | EPOLLWRBAND),
		POLLEX_SET  = (EPOLLERR | EPOLLPRI)
	};

	// enum {
	// 	POLLIN_SET  = (EPOLLRDNORM | EPOLLRDBAND | EPOLLIN | EPOLLHUP | EPOLLERR)
	// 	POLLOUT_SET = (EPOLLWRBAND | EPOLLWRNORM | EPOLLOUT | EPOLLERR)
	// 	POLLEX_SET =  (EPOLLPRI)
	// };

	int const mask = sock->ops->poll(0, sock, 0);

	struct lx_poll_result result = { false, false, false };

	if (mask & POLLIN_SET)
		result.in = true;
	if (mask & POLLOUT_SET)
		result.out = true;
	if (mask & POLLEX_SET)
		result.ex = true;

	return result;
}


int lx_sock_poll_wait(struct socket *socks[], unsigned num, int timeout)
{
	unsigned      i;
	unsigned long j;
	signed   long ex;
	unsigned int  to;

	enum { NUM_WQE = 8u, };
	struct wait_queue_entry sock_wqe[NUM_WQE];

	/* should not happen as the number of sockets is capped by libnl */
	if ((unsigned)NUM_WQE < num)
		printk("%s: more num: %d sockets than available"
		       "wait queue entries: %d\n", __func__, num, (unsigned)NUM_WQE);

	/*
	 * Add the appropriate wait queue entries and sleep afterwards
	 * for the requested timeout duration. Either a 'wake_up' call
	 * or the timeout will get us going again.
	 */

	__set_current_state(TASK_INTERRUPTIBLE);

	for (i = 0; i < num; i++) {
		struct socket *sock = socks[i];
		if (!sock)
			continue;

		init_waitqueue_entry(&sock_wqe[i], current);
		add_wait_queue(&(sock->sk->sk_wq->wait), &sock_wqe[i]);
	}

	j  = msecs_to_jiffies(timeout);
	ex = schedule_timeout(j);
	to = jiffies_to_msecs(ex);

	for (i = 0; i < num; i++) {
		struct socket *sock = socks[i];
		if (!sock)
			continue;

		remove_wait_queue(&(sock->sk->sk_wq->wait), &sock_wqe[i]);
	}

	return (int)to;
}
