/*
 * \brief  Lwip plugin implementation
 * \author Christian Prochaska 
 * \author Norman Feske
 * \date   2010-02-12
 */

/*
 * Copyright (C) 2010-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <base/env.h>
#include <base/log.h>

#define LWIP_COMPAT_SOCKETS 0

/* rename lwip functions that have the same name in the libc to lwip_*() */
#define addrinfo lwip_addrinfo
#define fd_set lwip_fd_set
#define hostent lwip_hostent
#define linger lwip_linger
#define sockaddr lwip_sockaddr
#define timeval lwip_timeval

/* the extern "C" declaration is missing in lwip/netdb.h */
extern "C" {
#include <lwip/netdb.h>
}
#include <lwip/genode.h>
#include <lwip/sockets.h>

/* lwip and libc have different definitions for the FD_* macros and renaming
 * of the macros is not possible, so wrapper functions are needed at this
 * place */

static inline void lwip_FD_ZERO(lwip_fd_set *set)
{
	FD_ZERO(set);
}

static inline bool lwip_FD_ISSET(int lwip_fd, lwip_fd_set *set)
{
	return FD_ISSET(lwip_fd, set);
}

static inline void lwip_FD_SET(int lwip_fd, lwip_fd_set *set)
{
	FD_SET(lwip_fd, set);
}

static constexpr long lwip_FIONBIO = FIONBIO;
static constexpr long lwip_FIONREAD = FIONREAD;
static constexpr int  lwip_O_NONBLOCK = O_NONBLOCK;

/* undefine lwip type names that are also defined in libc headers and have
 * been renamed to lwip_*() */
#undef addrinfo
#undef fd_set
#undef hostent
#undef linger
#undef sockaddr
#undef timeval

/* undefine lwip macros that are also defined in libc headers and cannot be
 * renamed */
#undef AF_INET6
#undef BIG_ENDIAN
#undef BYTE_ORDER
#undef FD_CLR
#undef FD_ISSET
#undef FD_SET
#undef FD_ZERO
#undef FIONBIO
#undef FIONREAD
#undef O_NDELAY
#undef O_NONBLOCK
#undef HOST_NOT_FOUND
#undef IOCPARM_MASK
#undef IOC_VOID
#undef IOC_OUT
#undef IOC_IN
#undef _IO
#undef _IOR
#undef _IOW
#undef LITTLE_ENDIAN
#undef MSG_DONTWAIT
#undef MSG_OOB
#undef MSG_PEEK
#undef MSG_WAITALL
#undef NO_DATA
#undef NO_RECOVERY
#undef EAI_FAIL
#undef EAI_MEMORY
#undef EAI_NONAME
#undef EAI_SERVICE
#undef SIOCATMARK
#undef SIOCGHIWAT
#undef SIOCGLOWAT
#undef SIOCSHIWAT
#undef SIOCSLOWAT
#undef SOL_SOCKET
#undef TRY_AGAIN


/**********************
 ** Plugin interface **
 **********************/

#include <libc-plugin/fd_alloc.h>
#include <libc-plugin/plugin_registry.h>

#include <assert.h>
#include <sys/ioctl.h>
#include <sys/fcntl.h>


namespace {


/**
 * For the lwip plugin, we only use the lwip file descriptor as context
 */
class Plugin_context : public Libc::Plugin_context
{
	private:

		int _lwip_fd;

	public:

		/**
		 * Constructor
		 *
		 * \param lwip_fd  file descriptor used for interacting with the
		 *                 socket API of lwip
		 */
		Plugin_context(int lwip_fd) : _lwip_fd(lwip_fd) { }

		int lwip_fd() const { return _lwip_fd; }
};


static inline Plugin_context *context(Libc::File_descriptor *fd)
{
	return static_cast<Plugin_context *>(fd->context);
}


static inline int get_lwip_fd(Libc::File_descriptor *sockfdo)
{
	return context(sockfdo)->lwip_fd();
}


struct Plugin : Libc::Plugin
{
	/**
	 * Constructor
	 */
	Plugin();

	bool supports_select(int nfds,
	                     fd_set *readfds,
	                     fd_set *writefds,
	                     fd_set *exceptfds,
	                     struct timeval *timeout) override;
	bool supports_socket(int domain, int type, int protocol) override;

	Libc::File_descriptor *accept(Libc::File_descriptor *sockfdo,
	                              struct sockaddr *addr,
	                              socklen_t *addrlen) override;
	int bind(Libc::File_descriptor *sockfdo,
	         const struct sockaddr *addr,
	         socklen_t addrlen) override;
	int close(Libc::File_descriptor *fdo) override;
	int connect(Libc::File_descriptor *sockfdo,
	            const struct sockaddr *addr,
	            socklen_t addrlen) override;
	int fcntl(Libc::File_descriptor *sockfdo, int cmd, long val) override;
	int getpeername(Libc::File_descriptor *sockfdo,
	                struct sockaddr *addr,
	                socklen_t *addrlen) override;
	int getsockname(Libc::File_descriptor *sockfdo,
	                struct sockaddr *addr,
	                socklen_t *addrlen) override;
	int getsockopt(Libc::File_descriptor *sockfdo, int level,
	               int optname, void *optval,
	               socklen_t *optlen) override;
	int ioctl(Libc::File_descriptor *sockfdo, int request, char *argp) override;
	int listen(Libc::File_descriptor *sockfdo, int backlog) override;
	ssize_t read(Libc::File_descriptor *fdo, void *buf, ::size_t count) override;
	int shutdown(Libc::File_descriptor *fdo, int) override;
	int select(int nfds, fd_set *readfds, fd_set *writefds,
	           fd_set *exceptfds, struct timeval *timeout) override;
	ssize_t send(Libc::File_descriptor *, const void *buf, ::size_t len, int flags) override;
	ssize_t sendto(Libc::File_descriptor *, const void *buf,
	               ::size_t len, int flags,
	               const struct sockaddr *dest_addr,
	               socklen_t addrlen) override;
	ssize_t recv(Libc::File_descriptor *, void *buf, ::size_t len, int flags) override;
	ssize_t recvfrom(Libc::File_descriptor *, void *buf, ::size_t len, int flags,
	                 struct sockaddr *src_addr, socklen_t *addrlen) override;
	int setsockopt(Libc::File_descriptor *sockfdo, int level,
	               int optname, const void *optval,
	               socklen_t optlen) override;
	Libc::File_descriptor *socket(int domain, int type, int protocol) override;
	ssize_t write(Libc::File_descriptor *fdo, const void *buf, ::size_t count) override;
};


Plugin::Plugin()
{
	Genode::log("using the lwIP libc plugin");

	lwip_tcpip_init();
}


bool Plugin::supports_select(int nfds,
                                  fd_set *readfds,
                                  fd_set *writefds,
                                  fd_set *exceptfds,
                                  struct timeval *timeout)
{
	Libc::File_descriptor *fdo;
	/* return true if any file descriptor which is marked set in one of
	 * the sets belongs to this plugin */
	for (int libc_fd = 0; libc_fd < nfds; libc_fd++) {
		if (FD_ISSET(libc_fd, readfds)
		 || FD_ISSET(libc_fd, writefds) || FD_ISSET(libc_fd, exceptfds)) {
			fdo = Libc::file_descriptor_allocator()->find_by_libc_fd(libc_fd);
			if (fdo && (fdo->plugin == this)) {
				return true;
			}
		}
	}
	return false;
}


bool Plugin::supports_socket(int domain, int, int)
{
	if (domain == AF_INET)
		return true;

	return false;
}


Libc::File_descriptor *Plugin::accept(Libc::File_descriptor *sockfdo,
                                           struct sockaddr *addr, socklen_t *addrlen)
{
	int lwip_fd = lwip_accept(get_lwip_fd(sockfdo), (struct lwip_sockaddr*)addr, addrlen);

	if (lwip_fd == -1) {
		return 0;
	}

	Plugin_context *context = new (Genode::env()->heap()) Plugin_context(lwip_fd);
	Libc::File_descriptor *fd = Libc::file_descriptor_allocator()->alloc(this, context);

	if (!fd)
		Genode::error("could not allocate file descriptor");

	return fd;
}


int Plugin::bind(Libc::File_descriptor *sockfdo, const struct sockaddr *addr,
                      socklen_t addrlen)
{
	return lwip_bind(get_lwip_fd(sockfdo), (struct lwip_sockaddr*)addr, addrlen);
}


int Plugin::close(Libc::File_descriptor *fdo)
{
	int result = lwip_close(get_lwip_fd(fdo));

	if (context(fdo))
		Genode::destroy(Genode::env()->heap(), context(fdo));
	Libc::file_descriptor_allocator()->free(fdo);

	return result;
}


int Plugin::connect(Libc::File_descriptor *sockfdo,
                         const struct sockaddr *addr,
                         socklen_t addrlen)
{
	return lwip_connect(get_lwip_fd(sockfdo), (struct lwip_sockaddr*)addr, addrlen);
}


int Plugin::fcntl(Libc::File_descriptor *sockfdo, int cmd, long val)
{
	int s = get_lwip_fd(sockfdo);
	int result = -1;

	switch (cmd) {
	case F_GETFL:
		/* lwip_fcntl() supports only the 'O_NONBLOCK' flag */
		result = lwip_fcntl(s, cmd, val);
		if (result == lwip_O_NONBLOCK)
			result = O_NONBLOCK;
		break;
	case F_SETFL:
		/*
		 * lwip_fcntl() supports only the 'O_NONBLOCK' flag and only if
		 * no other flag is set.
		 */
		result = lwip_fcntl(s, cmd, (val & O_NONBLOCK) ? lwip_O_NONBLOCK : 0);
		break;
	default:
		Genode::error("libc_lwip: unsupported fcntl() request: ", cmd);
		break;
	}

	return result;
}


int Plugin::getpeername(Libc::File_descriptor *sockfdo,
                             struct sockaddr *addr,
                             socklen_t *addrlen)
{
	return lwip_getpeername(get_lwip_fd(sockfdo), (struct lwip_sockaddr*)addr, addrlen);
}


int Plugin::getsockname(Libc::File_descriptor *sockfdo,
                             struct sockaddr *addr,
                             socklen_t *addrlen)
{
	return lwip_getsockname(get_lwip_fd(sockfdo), (struct lwip_sockaddr*)addr, addrlen);
}


int Plugin::getsockopt(Libc::File_descriptor *sockfdo, int level,
                            int optname, void *optval, socklen_t *optlen)
{
	return lwip_getsockopt(get_lwip_fd(sockfdo), level, optname, optval, optlen);
}

int Plugin::ioctl(Libc::File_descriptor *sockfdo, int request, char *argp)
{
	switch (request) {
	case FIONBIO:
		return lwip_ioctl(get_lwip_fd(sockfdo), lwip_FIONBIO, argp);
		break;
	case FIONREAD:
		return lwip_ioctl(get_lwip_fd(sockfdo), lwip_FIONREAD, argp);;
		break;
	default:
		Genode::error("unsupported ioctl() request");
		errno = ENOSYS;
		return -1;
	}
}


int Plugin::listen(Libc::File_descriptor *sockfdo, int backlog)
{
	return lwip_listen(get_lwip_fd(sockfdo), backlog);
}


ssize_t Plugin::read(Libc::File_descriptor *fdo, void *buf, ::size_t count)
{
	return lwip_read(get_lwip_fd(fdo), buf, count);
}


int Plugin::shutdown(Libc::File_descriptor *sockfdo, int how)
{
	return lwip_shutdown(get_lwip_fd(sockfdo), how);
}


int Plugin::select(int nfds,
                        fd_set *readfds,
                        fd_set *writefds,
                        fd_set *exceptfds,
                        struct timeval *timeout)
{
	Libc::File_descriptor *fdo;
	lwip_fd_set lwip_readfds;
	lwip_fd_set lwip_writefds;
	lwip_fd_set lwip_exceptfds;
	int libc_fd;
	int lwip_fd;
	int highest_lwip_fd = -2;
	int result;

	lwip_FD_ZERO(&lwip_readfds);
	lwip_FD_ZERO(&lwip_writefds);
	lwip_FD_ZERO(&lwip_exceptfds);

	for (libc_fd = 0; libc_fd < nfds; libc_fd++) {

		fdo = Libc::file_descriptor_allocator()->find_by_libc_fd(libc_fd);

		/* handle only libc_fds that belong to this plugin */
		if (!fdo || (fdo->plugin != this))
			continue;

		if (FD_ISSET(libc_fd, readfds) ||
			FD_ISSET(libc_fd, writefds) ||
			FD_ISSET(libc_fd, exceptfds)) {

			lwip_fd = get_lwip_fd(fdo);

			if (lwip_fd > highest_lwip_fd) {
				highest_lwip_fd = lwip_fd;
			}

			if (FD_ISSET(libc_fd, readfds)) {
				lwip_FD_SET(lwip_fd, &lwip_readfds);
			}

			if (FD_ISSET(libc_fd, writefds)) {
				lwip_FD_SET(lwip_fd, &lwip_writefds);
			}

			if (FD_ISSET(libc_fd, exceptfds)) {
				lwip_FD_SET(lwip_fd, &lwip_exceptfds);
			}
		}
	}

	result = lwip_select(highest_lwip_fd + 1,
			             &lwip_readfds,
			             &lwip_writefds,
			             &lwip_exceptfds,
			             (struct lwip_timeval*)timeout);

	if (result > 0) {

		/* clear result sets */
		FD_ZERO(readfds);
		FD_ZERO(writefds);
		FD_ZERO(exceptfds);

		for (lwip_fd = 0; lwip_fd <= highest_lwip_fd; lwip_fd++) {

			/* find an lwip_fd which is in the set */

			if (lwip_FD_ISSET(lwip_fd, &lwip_readfds) ||
				lwip_FD_ISSET(lwip_fd, &lwip_writefds) ||
				lwip_FD_ISSET(lwip_fd, &lwip_exceptfds)) {

				/* find matching libc_fd for lwip_fd */
				for (libc_fd = 0; libc_fd < nfds; libc_fd++) {

					fdo = Libc::file_descriptor_allocator()->find_by_libc_fd(libc_fd);

					if (fdo && (fdo->plugin == this) &&
					   (get_lwip_fd(fdo) == lwip_fd)) {

						if (lwip_FD_ISSET(lwip_fd, &lwip_readfds)) {
							FD_SET(libc_fd, readfds);
						}

						if (lwip_FD_ISSET(lwip_fd, &lwip_writefds)) {
							FD_SET(libc_fd, writefds);
						}

						if (lwip_FD_ISSET(lwip_fd, &lwip_exceptfds)) {
							FD_SET(libc_fd, exceptfds);
						}
					}
				}
			}
		}
	}

	return result;
}


ssize_t Plugin::recv(Libc::File_descriptor *sockfdo, void *buf, ::size_t len, int flags)
{
	return lwip_recv(get_lwip_fd(sockfdo), buf, len, flags);
}


ssize_t Plugin::recvfrom(Libc::File_descriptor *sockfdo, void *buf, ::size_t len, int flags,
                         struct sockaddr *src_addr, socklen_t *addrlen)
{
	return lwip_recvfrom(get_lwip_fd(sockfdo), buf, len, flags,
	                     (struct lwip_sockaddr *)src_addr, addrlen);
}


ssize_t Plugin::send(Libc::File_descriptor *sockfdo, const void *buf, ::size_t len, int flags)
{
	return lwip_send(get_lwip_fd(sockfdo), buf, len, flags);
}


ssize_t Plugin::sendto(Libc::File_descriptor *sockfdo, const void *buf,
                       ::size_t len, int flags,
                       const struct sockaddr *dest_addr, socklen_t addrlen)
{
	return lwip_sendto(get_lwip_fd(sockfdo), buf, len, flags,
	                   (struct lwip_sockaddr *)dest_addr, addrlen);
}


int Plugin::setsockopt(Libc::File_descriptor *sockfdo, int level,
                       int optname, const void *optval,
                       socklen_t optlen)
{
	return lwip_setsockopt(get_lwip_fd(sockfdo), level, optname, optval, optlen);
}


Libc::File_descriptor *Plugin::socket(int domain, int type, int protocol)
{
	int lwip_fd = lwip_socket(domain, type, protocol);

	if (lwip_fd == -1) {
		Genode::error("lwip_socket() failed");
		return 0;
	}

	Plugin_context *context = new (Genode::env()->heap()) Plugin_context(lwip_fd);
	return Libc::file_descriptor_allocator()->alloc(this, context);
}


ssize_t Plugin::write(Libc::File_descriptor *fdo, const void *buf, ::size_t count)
{
	return lwip_write(get_lwip_fd(fdo), buf, count);
}


} /* unnamed namespace */


void create_lwip_plugin()
{
	static Plugin lwip_plugin;
}
