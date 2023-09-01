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


#include <genode_c_api/socket.h>

#ifdef __cplusplus
extern "C" {
#endif

struct socket *lx_sock_alloc(void);
void           lx_sock_release(struct socket* sock);

void lx_socket_address(struct genode_socket_config *config);
void lx_socket_mtu(unsigned mtu);

enum Errno lx_socket_create(int domain, int type, int protocol, struct socket **res);
enum Errno lx_socket_bind(struct socket *sock, struct genode_sockaddr const *addr);
enum Errno lx_socket_listen(struct socket *sock, int length);
enum Errno lx_socket_accept(struct socket *sock, struct socket *new_sock,
                            struct genode_sockaddr *addr);
enum Errno lx_socket_connect(struct socket *sock, struct genode_sockaddr const *addr);

unsigned lx_socket_pollin_set(void);
unsigned lx_socket_pollout_set(void);
unsigned lx_socket_pollex_set(void);
unsigned lx_socket_poll(struct socket *sock);

enum Errno lx_socket_getsockopt(struct socket *sock, enum Sock_level level,
                                enum Sock_opt opt, void *optval, unsigned *optlen);
enum Errno lx_socket_setsockopt(struct socket *sock, enum Sock_level level,
                                enum Sock_opt opt, void const *optval, unsigned optlen);

enum Errno lx_socket_getname(struct socket *sock, struct genode_sockaddr *addr, bool peer);
enum Errno lx_socket_sendmsg(struct socket *sock, struct genode_msghdr *msg,
                             unsigned long *bytes_send);
enum Errno lx_socket_recvmsg(struct socket *sock, struct genode_msghdr *msg,
                             unsigned long *bytes_recv, bool peek);

enum Errno lx_socket_shutdown(struct socket *sock, int how);
enum Errno lx_socket_release(struct socket *sock);

#ifdef __cplusplus
} /* extern "C" */
#endif
