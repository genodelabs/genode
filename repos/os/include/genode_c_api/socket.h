/*
 * \brief  Genode socket C-API
 * \author Sebastian Sumpf
 * \date   2024-01-29
 */

/*
 * Copyright (C) 2024 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <base/fixed_stdint.h>
#include <genode_c_api/base.h>

/* define required types in case they are not present */
#ifndef AF_UNSPEC
#define AF_UNSPEC 0
#endif

#ifndef AF_INET
#define AF_INET 2
#endif

#ifndef SOCK_STREAM
#define SOCK_STREAM 1
#endif

#ifndef SOCK_DGRAM
#define SOCK_DGRAM 2
#endif

#ifdef __cplusplus
extern "C" {
#endif

enum Errno {
	GENODE_ENONE = 0,
	GENODE_E2BIG = 1,
	GENODE_EACCES = 2,
	GENODE_EADDRINUSE = 3,
	GENODE_EADDRNOTAVAIL = 4,
	GENODE_EAFNOSUPPORT = 5,
	GENODE_EAGAIN = 6,
	GENODE_EALREADY = 7,
	GENODE_EBADF = 8,
	GENODE_EBADFD = 9,
	GENODE_EBADMSG = 10,
	GENODE_EBADRQC = 11,
	GENODE_EBUSY = 12,
	GENODE_ECONNABORTED = 13,
	GENODE_ECONNREFUSED = 14,
	GENODE_EDESTADDRREQ = 15,
	GENODE_EDOM = 16,
	GENODE_EEXIST = 17,
	GENODE_EFAULT = 18,
	GENODE_EFBIG = 19,
	GENODE_EHOSTUNREACH = 20,
	GENODE_EINPROGRESS = 21,
	GENODE_EINTR = 22,
	GENODE_EINVAL = 23,
	GENODE_EIO = 24,
	GENODE_EISCONN = 25,
	GENODE_ELOOP = 26,
	GENODE_EMLINK = 27,
	GENODE_EMSGSIZE = 28,
	GENODE_ENAMETOOLONG = 29,
	GENODE_ENETDOWN = 30,
	GENODE_ENETUNREACH = 31,
	GENODE_ENFILE = 32,
	GENODE_ENOBUFS = 33,
	GENODE_ENODATA = 34,
	GENODE_ENODEV = 35,
	GENODE_ENOENT = 36,
	GENODE_ENOIOCTLCMD = 37,
	GENODE_ENOLINK = 38,
	GENODE_ENOMEM = 39,
	GENODE_ENOMSG = 40,
	GENODE_ENOPROTOOPT = 41,
	GENODE_ENOSPC = 42,
	GENODE_ENOSYS = 43,
	GENODE_ENOTCONN = 44,
	GENODE_ENOTSUPP = 45,
	GENODE_ENOTTY = 46,
	GENODE_ENXIO = 47,
	GENODE_EOPNOTSUPP = 48,
	GENODE_EOVERFLOW = 49,
	GENODE_EPERM = 50,
	GENODE_EPFNOSUPPORT = 51,
	GENODE_EPIPE = 52,
	GENODE_EPROTO = 53,
	GENODE_EPROTONOSUPPORT = 54,
	GENODE_EPROTOTYPE = 55,
	GENODE_ERANGE = 56,
	GENODE_EREMCHG = 57,
	GENODE_ESOCKTNOSUPPORT = 58,
	GENODE_ESPIPE = 59,
	GENODE_ESRCH = 60,
	GENODE_ESTALE = 61,
	GENODE_ETIMEDOUT = 62,
	GENODE_ETOOMANYREFS = 63,
	GENODE_EUSERS = 64,
	GENODE_EXDEV = 65,
	GENODE_ECONNRESET = 66,
	GENODE_MAX_ERRNO = 67,
};

enum Sock_opt {
	/* found in lxip and lwip */
	GENODE_SO_DEBUG = 1,
	GENODE_SO_ACCEPTCONN = 2,
	GENODE_SO_DONTROUTE = 3,
	GENODE_SO_LINGER = 4,
	GENODE_SO_OOBINLINE = 5,
	GENODE_SO_REUSEPORT = 6,
	GENODE_SO_SNDBUF = 7,
	GENODE_SO_RCVBUF = 8,
	GENODE_SO_SNDLOWAT = 9,
	GENODE_SO_RCVLOWAT = 10,
	GENODE_SO_SNDTIMEO = 11,
	GENODE_SO_RCVTIMEO = 12,
	GENODE_SO_ERROR = 13,
	GENODE_SO_TYPE = 14,
	GENODE_SO_BINDTODEVICE = 15,
	GENODE_SO_BROADCAST = 16,
};

enum Sock_level {
	GENODE_SOL_SOCKET = 1,
};

struct genode_socket_handle;

struct genode_sockaddr
{
	genode_uint16_t family;
	union {
		/* AF_INET (or IPv4) */
		struct {
			genode_uint16_t port; /* big endian = network byte order */
			genode_uint32_t addr; /* big endian = network byte order */
		} in;
	};
};

/*
 * I/O progress callback can be registered via genode_socket_init and is
 * executed when possible progress (e.g., packet received) has been made
 */
struct genode_socket_io_progress
{
	void  *data;
	void (*callback)(void *);
};


bool genode_socket_init(struct genode_env *env,
                        struct genode_socket_io_progress *,
                        char const *label);


/*
 * Wakeup remote peers. This can be used as a callback for triggering, for
 * example, signal submission of the packet stream
 */
struct genode_socket_wakeup {
	void  *data;
	void (*callback)(void *);
};

void genode_socket_register_wakeup(struct genode_socket_wakeup *);
void genode_socket_wakeup_remote(void);

/*
 * IPv4 address configuration (DHCP or static)
 */
struct genode_socket_config
{
	/* IPv4 */
	bool dhcp;
	char const *ip_addr;
	char const *netmask;
	char const *gateway;
	char const *nameserver;
};

/*
 * Configure/obtain IP address (blocking)
 */
void genode_socket_config_address(struct genode_socket_config *config);

/*
 * Retrieve IPv4 configuration
 */

struct genode_socket_info
{
	/* all big endian */
	unsigned ip_addr;
	unsigned netmask;
	unsigned gateway;
	unsigned nameserver;
	bool     link_state;
};

void genode_socket_config_info(struct genode_socket_info *info);

/*
 * Configure MTU size (default should be 1500)
 */
void genode_socket_configure_mtu(unsigned mtu);


/*
 * Wait for I/O progress (synchronous) - used for testing if no
 * genode_socket_io_progress has been registered.
 */
void genode_socket_wait_for_progress(void);


/*
 * The following calls have POSIX semantics and are non-blocking
 */

struct genode_socket_handle *
genode_socket(int domain, int type, int protocol, enum Errno *);

enum Errno genode_socket_bind(struct genode_socket_handle *,
                              struct genode_sockaddr const *);

enum Errno genode_socket_listen(struct genode_socket_handle *,
                                int backlog);
struct genode_socket_handle *
genode_socket_accept(struct genode_socket_handle *,
                     struct genode_sockaddr *,
                     enum Errno *);

enum Errno genode_socket_connect(struct genode_socket_handle *,
                                 struct genode_sockaddr *);

unsigned genode_socket_pollin_set(void);
unsigned genode_socket_pollout_set(void);
unsigned genode_socket_pollex_set(void);

unsigned genode_socket_poll(struct genode_socket_handle *);

enum Errno genode_socket_getsockopt(struct genode_socket_handle *,
                                    enum Sock_level, enum Sock_opt,
                                    void *optval, unsigned *optlen);

enum Errno genode_socket_setsockopt(struct genode_socket_handle *,
                                    enum Sock_level, enum Sock_opt,
                                    void const *optval,
                                    unsigned optlen);

enum Errno genode_socket_getsockname(struct genode_socket_handle *,
                                     struct genode_sockaddr *);

enum Errno genode_socket_getpeername(struct genode_socket_handle *,
                                     struct genode_sockaddr *);

/*
 * I/O vector
 */
struct genode_iovec
{
	void         *base;
	unsigned long size;
	unsigned long used;
};


struct genode_msghdr
{
	struct genode_sockaddr *name;   /* can be NULL for TCP    */
	struct genode_iovec    *iov;    /* array of iovecs        */
	unsigned long           iovlen; /* nr elements in msg_iov */
};


enum Errno genode_socket_sendmsg(struct genode_socket_handle *,
                                 struct genode_msghdr *,
                                 unsigned long *bytes_send);

/**
 * \param msg_peek  Does not advance data read pointer, so data can be re-read
 *                  later
 */
enum Errno genode_socket_recvmsg(struct genode_socket_handle *,
                                 struct genode_msghdr *,
                                 unsigned long *bytes_recv,
                                 bool msg_peek);

enum Errno genode_socket_shutdown(struct genode_socket_handle *,
                                  int how);

enum Errno genode_socket_release(struct genode_socket_handle *);

#ifdef __cplusplus
} /* extern "C" */
#endif
