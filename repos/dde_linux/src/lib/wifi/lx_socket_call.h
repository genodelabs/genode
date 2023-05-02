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

#ifndef _LX_SOCKET_CALL_H_
#define _LX_SOCKET_CALL_H_

#ifdef __cplusplus
extern "C" {
#endif

struct lx_iov
{
	void          *iov_base;
	unsigned long  iov_len;
};

enum { MAX_IOV_LEN = 8 };


struct lx_msghdr
{
	void          *msg_name;
	unsigned       msg_namelen;
	struct lx_iov  msg_iov[MAX_IOV_LEN];
	unsigned       msg_iovcount;
	/*
	 * Omit total length covered by vector as it is
	 * calculated where needed.
	 */

	void          *msg_control;
	unsigned       msg_controllen;
};

struct socket;

int lx_sock_create_kern(int domain, int type, int protocol,
                        struct socket **res);
int lx_sock_release(struct socket *sock);
int lx_sock_bind(struct socket *sock, void *sockaddr, int sockaddr_len);
int lx_sock_getname(struct socket *sock, void *sockaddr, int peer);
int lx_sock_recvmsg(struct socket *sock, struct lx_msghdr *lx_msg,
                    int flags, int dontwait);
int lx_sock_sendmsg(struct socket *socket, struct lx_msghdr* lx_msg,
                    int flags, int dontwait);
int lx_sock_setsockopt(struct socket *sock, int level, int optname,
                       void const *optval, unsigned optlen);
unsigned char const* lx_get_mac_addr(void);

struct lx_poll_result
{
	int in;
	int out;
	int ex;
};

struct lx_poll_result lx_sock_poll(struct socket *sock);

int lx_sock_poll_wait(struct socket *sock[], unsigned num, int timeout);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* _LX_SOCKET_CALL_H_ */
