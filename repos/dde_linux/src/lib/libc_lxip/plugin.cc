/*
 * \brief  Lxip plugin implementation
 * \author Sebastian Sumpf
 * \author Christian Helmuth
 * \date   2013-09-04
 */

/*
 * Copyright (C) 2010-2017 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

/* Libc includes */
#include <errno.h>
#include <netinet/in.h>
#include <sys/fcntl.h>
#include <sys/ioctl.h>

/* Genode includes */
#include <base/attached_rom_dataspace.h>
#include <base/env.h>
#include <base/heap.h>
#include <base/log.h>

/* Libc plugin includes */
#include <libc-plugin/fd_alloc.h>
#include <libc-plugin/plugin_registry.h>

/* Lxip includes */
#include <lxip/lxip.h>


/**************************
 ** Linux family numbers **
 **************************/

enum {
	LINUX_AF_INET = 2
};


/**********************
 ** Plugin interface **
 **********************/

namespace {

class Plugin_context : public Libc::Plugin_context
{
	private:

		Lxip::Handle _handle;

	public:

		/**
		 * Constructor
		 */
		Plugin_context(Lxip::Handle handle) : _handle(handle) { }

		Lxip::Handle handle() const { return _handle; }

		void non_block(bool nb) { _handle.non_block = nb; }
};


static inline Plugin_context * context(Libc::File_descriptor *fd)
{
	return static_cast<Plugin_context *>(fd->context);
}


struct Plugin : Libc::Plugin
{

	/**
	 * Interface to LXIP stack
	 */
	struct Socketcall
	{
		Genode::Heap      heap;
		Lxip::Socketcall &socketcall;

		Socketcall(Genode::Env &env,
		           char const  *ip_addr_str,
		           char const  *netmask_str,
		           char const  *gateway_str)
		:  heap(env.ram(), env.rm()),
		   socketcall(Lxip::init(env, ip_addr_str, netmask_str,
		                         gateway_str, gateway_str))
		{ }
	};

	Genode::Constructible<Socketcall> socketconstruct;

	Lxip::Socketcall &socketcall()
	{
		return socketconstruct->socketcall;
	}

	Genode::Heap &heap()
	{
		return socketconstruct->heap;
	}

	/**
	 * Constructor
	 */
	Plugin();

	void init(Genode::Env &env) override;

	bool supports_select(int nfds,
	                     fd_set *readfds,
	                     fd_set *writefds,
	                     fd_set *exceptfds,
	                     struct timeval *timeout);
	bool supports_socket(int domain, int type, int protocol);

	Libc::File_descriptor *accept(Libc::File_descriptor *sockfdo,
	                              struct sockaddr *addr,
	                              socklen_t *addrlen);
	int bind(Libc::File_descriptor *sockfdo,
	         const struct sockaddr *addr,
	         socklen_t addrlen);
	int close(Libc::File_descriptor *fdo);
	int connect(Libc::File_descriptor *sockfdo,
	            const struct sockaddr *addr,
	            socklen_t addrlen);
	int fcntl(Libc::File_descriptor *sockfdo, int cmd, long val);
	int getpeername(Libc::File_descriptor *sockfdo,
	                struct sockaddr *addr,
	                socklen_t *addrlen);
	int getsockname(Libc::File_descriptor *sockfdo,
	                struct sockaddr *addr,
	                socklen_t *addrlen);
	int getsockopt(Libc::File_descriptor *sockfdo, int level,
	               int optname, void *optval,
	               socklen_t *optlen);
	int ioctl(Libc::File_descriptor *sockfdo, int request, char *argp);
	int listen(Libc::File_descriptor *sockfdo, int backlog);
	ssize_t read(Libc::File_descriptor *fdo, void *buf, ::size_t count);
	int shutdown(Libc::File_descriptor *fdo, int);
	int select(int nfds, fd_set *readfds, fd_set *writefds,
	           fd_set *exceptfds, struct timeval *timeout);
	ssize_t send(Libc::File_descriptor *, const void *buf, ::size_t len, int flags);
	ssize_t sendto(Libc::File_descriptor *, const void *buf,
	               ::size_t len, int flags,
	               const struct sockaddr *dest_addr,
	               socklen_t addrlen);
	ssize_t recv(Libc::File_descriptor *, void *buf, ::size_t len, int flags);
	ssize_t recvfrom(Libc::File_descriptor *, void *buf, ::size_t len, int flags,
	                 struct sockaddr *src_addr, socklen_t *addrlen);
	int setsockopt(Libc::File_descriptor *sockfdo, int level,
	               int optname, const void *optval,
	               socklen_t optlen);
	Libc::File_descriptor *socket(int domain, int type, int protocol);
	ssize_t write(Libc::File_descriptor *fdo, const void *buf, ::size_t count);

	int  linux_family(const struct sockaddr *addr, socklen_t addrlen);
	int  bsd_family(struct sockaddr *addr);
	int  retrieve_and_clear_fds(int nfds, struct fd_set *fds, struct fd_set *in);
	int  translate_msg_flags(int bsd_flags);
	int  translate_ops_linux(int optname);

	::ssize_t getdirentries(Libc::File_descriptor *fd, char *buf, ::size_t nbytes,
	                        ::off_t *basep)
	{
		Genode::error(__FILE__, ":", __LINE__, " ", __func__, "not implemented");
		return 0UL;
	}

	void *mmap(void *addr, ::size_t length, int prot, int flags,
	           Libc::File_descriptor *, ::off_t offset)
	{
		Genode::error(__FILE__, ":", __LINE__, " ", __func__, "not implemented");
		return nullptr;
	}

	int msync(void *addr, ::size_t len, int flags)
	{
		Genode::error(__FILE__, ":", __LINE__, " ", __func__, "not implemented");
		return 0;
	}
};


Plugin::Plugin()
{
	Genode::log("using the lxip libc plugin");
}


void Plugin::init(Genode::Env &env)
{
	char ip_addr_str[16] = {0};
	char netmask_str[16] = {0};
	char gateway_str[16] = {0};

	Genode::Attached_rom_dataspace config { env, "config"} ;

	try {
		Genode::Xml_node libc_node = config.xml().sub_node("libc");

		try {
			libc_node.attribute("ip_addr").value(ip_addr_str, sizeof(ip_addr_str));
		} catch(...) { }

		try {
			libc_node.attribute("netmask").value(netmask_str, sizeof(netmask_str));
		} catch(...) { }

		try {
			libc_node.attribute("gateway").value(gateway_str, sizeof(gateway_str));
		} catch(...) { }

		/* either none or all 3 interface attributes must exist */
		if ((Genode::strlen(ip_addr_str) != 0) ||
		    (Genode::strlen(netmask_str) != 0) ||
		    (Genode::strlen(gateway_str) != 0)) {
			if (Genode::strlen(ip_addr_str) == 0) {
				Genode::error("missing \"ip_addr\" attribute. Ignoring network interface config.");
				throw Genode::Xml_node::Nonexistent_attribute();
			} else if (Genode::strlen(netmask_str) == 0) {
				Genode::error("missing \"netmask\" attribute. Ignoring network interface config.");
				throw Genode::Xml_node::Nonexistent_attribute();
			} else if (Genode::strlen(gateway_str) == 0) {
				Genode::error("missing \"gateway\" attribute. Ignoring network interface config.");
				throw Genode::Xml_node::Nonexistent_attribute();
			}
		} else
			throw -1;

		Genode::log("static network interface: ",
		            "ip_addr=", Genode::Cstring(ip_addr_str), " "
		            "netmask=", Genode::Cstring(netmask_str), " "
		            "gateway=", Genode::Cstring(gateway_str));
	}
	catch (...) {
		Genode::log("Using DHCP for interface configuration.");
	}

	socketconstruct.construct(env, ip_addr_str, netmask_str, gateway_str);
};

/* TODO shameful copied from lwip... generalize this */
bool Plugin::supports_select(int             nfds,
                             fd_set         *readfds,
                             fd_set         *writefds,
                             fd_set         *exceptfds,
                             struct timeval *timeout)
{
	/*
	 * Return true if any file descriptor which is marked set in one of
	 * the sets belongs to this plugin.
	 */

	Libc::File_descriptor *fdo;
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


bool Plugin::supports_socket(int domain, int type, int)
{
	if (domain == AF_INET && (type == SOCK_STREAM || type == SOCK_DGRAM))
		return true;

	return false;
}


Libc::File_descriptor *Plugin::accept(Libc::File_descriptor *sockfdo,
                                      struct sockaddr *addr, socklen_t *addrlen)
{
	Lxip::Handle handle;
	
	handle = socketcall().accept(context(sockfdo)->handle(), (void *)addr, addrlen);
	if (!handle.socket)
		return 0;

	if (addr) {
		addr->sa_family = bsd_family(addr);
		addr->sa_len    = *addrlen;
	}

	Plugin_context *context   = new (heap()) Plugin_context(handle);
	Libc::File_descriptor *fd = Libc::file_descriptor_allocator()->alloc(this, context);
	return fd;
}


int Plugin::bind(Libc::File_descriptor *sockfdo, const struct sockaddr *addr,
                 socklen_t addrlen)
{
	int family;

	if (!(family = linux_family(addr, addrlen))) {
		errno = ENOTSUP;
		return -1;
	}

	errno = -socketcall().bind(context(sockfdo)->handle(), family, (void*)addr);

	return errno > 0 ? -1 : 0;
}


int Plugin::close(Libc::File_descriptor *sockfdo)
{
	socketcall().close(context(sockfdo)->handle());

	if (context(sockfdo))
		Genode::destroy(heap(), context(sockfdo));

	Libc::file_descriptor_allocator()->free(sockfdo);

	return 0;
}


int Plugin::connect(Libc::File_descriptor *sockfdo,
                    const struct sockaddr *addr,
                    socklen_t addrlen)
{
	int family;

	if (!(family = linux_family(addr, addrlen))) {
		errno = ENOTSUP;
		return -1;
	}

	errno = -socketcall().connect(context(sockfdo)->handle(), family, (void *)addr);
	return errno > 0 ? -1 : 0;
}


int Plugin::fcntl(Libc::File_descriptor *sockfdo, int cmd, long val)
{

	switch (cmd) {
		
		case F_GETFL:

			return context(sockfdo)->handle().non_block ? O_NONBLOCK : 0;

		case F_SETFL:

			context(sockfdo)->non_block(!!(val & O_NONBLOCK));
			return 0;

		default:

			Genode::error("unsupported fcntl() request: ", cmd);
			errno = ENOSYS;
			return -1;
	}
}


int Plugin::getpeername(Libc::File_descriptor *sockfdo,
                             struct sockaddr *addr,
                             socklen_t *addrlen)
{
	if (!addr) {
		errno = EFAULT;
		return -1;
	}

	errno = -socketcall().getpeername(context(sockfdo)->handle(), (void *)addr, addrlen);

	addr->sa_family = bsd_family(addr);
	addr->sa_len    = *addrlen;

	return errno > 0 ? -1 : 0;
}


int Plugin::getsockname(Libc::File_descriptor *sockfdo,
                             struct sockaddr *addr,
                             socklen_t *addrlen)
{
	if (!addr) {
		errno = EFAULT;
		return -1;
	}

	errno = -socketcall().getsockname(context(sockfdo)->handle(), (void *)addr, addrlen);

	addr->sa_family = bsd_family(addr);
	addr->sa_len    = *addrlen;

	return errno > 0 ? -1 : 0;
}


int Plugin::getsockopt(Libc::File_descriptor *sockfdo, int level,
                            int optname, void *optval, socklen_t *optlen)
{
	if (level != SOL_SOCKET) {
		Genode::error(__func__, ": Unsupported level ", level, ", we only support SOL_SOCKET for now");
		errno = EBADF;
		return -1;
	}

	optname = translate_ops_linux(optname);
	if (optname < 0) {
		errno = ENOPROTOOPT;
		return -1;
	}

	return socketcall().getsockopt(context(sockfdo)->handle(), Lxip::LINUX_SOL_SOCKET,
	                             optname, optval, (int *)optlen);
}


int Plugin::ioctl(Libc::File_descriptor *sockfdo, int request, char *argp)
{
	switch (request) {

		case FIONBIO:

			context(sockfdo)->non_block(!!*argp);
			return 0;

		case FIONREAD:

			errno = -socketcall().ioctl(context(sockfdo)->handle(), Lxip::LINUX_FIONREAD,
			                          argp);
			return errno > 0 ? -1 : 0;

		default:

			Genode::error("unsupported ioctl() request");
			errno = ENOSYS;
			return -1;
	}
}


int Plugin::listen(Libc::File_descriptor *sockfdo, int backlog)
{
	errno = -socketcall().listen(context(sockfdo)->handle(), backlog);
	return errno > 0 ? -1 : 0;
}


int Plugin::shutdown(Libc::File_descriptor *sockfdo, int how)
{
	errno = -socketcall().shutdown(context(sockfdo)->handle(), how);
	return errno > 0 ? -1 : 0;
}


/* TODO: Support timeouts */
/* XXX: Check blocking and non-blocking semantics */
int Plugin::select(int nfds,
                   fd_set *readfds,
                   fd_set *writefds,
                   fd_set *exceptfds,
                   struct timeval *timeout)
{
	Libc::File_descriptor *sockfdo;
	struct fd_set fds[3];
	int bits = 0;

	if (nfds < 0) {
		errno = EINVAL;
		return -1;
	}

	for (int i = 0; i < 3; i++)
		FD_ZERO(&fds[i]);

	bool block = false;
	for (int fd = 0; fd <= nfds; fd++) {

		if (fd == nfds && !bits) {
			fd    = -1;
			block = true;
			continue;
		}

		int set = 0;
		set |= FD_ISSET(fd, readfds);
		set |= FD_ISSET(fd, writefds);
		set |= FD_ISSET(fd, exceptfds);

		if (!set)
			continue;

		sockfdo = Libc::file_descriptor_allocator()->find_by_libc_fd(fd);
		/* handle only libc_fds that belong to this plugin */
		if (!sockfdo || (sockfdo->plugin != this))
			continue;

		/* call IP stack blocking/non-blocking */
		int mask = socketcall().poll(context(sockfdo)->handle(), block);

		if (mask)
			block = false;

		if (readfds && (mask & Lxip::POLLIN) && FD_ISSET(fd, readfds) && ++bits)
			FD_SET(fd, &fds[0]);
		if (writefds  && (mask & Lxip::POLLOUT) && FD_ISSET(fd, writefds) && ++bits)
			FD_SET(fd, &fds[1]);
		if (exceptfds && (mask & Lxip::POLLEX) && FD_ISSET(fd, exceptfds) && ++bits)
			FD_SET(fd, &fds[2]);
	}

	if (readfds)   *readfds   = fds[0];
	if (writefds)  *writefds  = fds[1];
	if (exceptfds) *exceptfds = fds[2];

	return bits;
}


ssize_t Plugin::read(Libc::File_descriptor *fdo, void *buf, ::size_t count)
{
	return recv(fdo, buf, count, 0);
}


ssize_t Plugin::recv(Libc::File_descriptor *sockfdo, void *buf, ::size_t len, int flags)
{
	return recvfrom(sockfdo, buf, len, flags, 0, 0);
}


ssize_t Plugin::recvfrom(Libc::File_descriptor *sockfdo, void *buf, ::size_t len, int flags,
                         struct sockaddr *src_addr, socklen_t *addrlen)
{
	int family = 0;

	if (src_addr && addrlen && !(family = linux_family(src_addr, *addrlen))) {
		errno = ENOTSUP;
		return -1;
	}

	int recv =  socketcall().recv(context(sockfdo)->handle(), buf, len, translate_msg_flags(flags),
	                            family, (void *)src_addr, addrlen);

	if (recv < 0) {
		errno = -recv;
		return errno == EAGAIN ? 0 : -1;
	}

	if (src_addr) {
		src_addr->sa_family = bsd_family(src_addr);
		src_addr->sa_len    = *addrlen;
	}

	return recv;
}


ssize_t Plugin::send(Libc::File_descriptor *sockfdo, const void *buf, ::size_t len, int flags)
{
	return sendto(sockfdo, buf, len, flags, 0, 0);
}


ssize_t Plugin::sendto(Libc::File_descriptor *sockfdo, const void *buf,
                       ::size_t len, int flags,
                       const struct sockaddr *dest_addr, socklen_t addrlen)
{
	int family = 0;

	if (dest_addr && addrlen && !(family = linux_family(dest_addr, addrlen))) {
		errno = ENOTSUP;
		return -1;
	}

	int send =  socketcall().send(context(sockfdo)->handle(), buf, len, translate_msg_flags(flags),
	                            family, (void *)dest_addr);
	if (send < 0)
		errno = -send;

	return send < 0 ? -1 : send;
}


int Plugin::setsockopt(Libc::File_descriptor *sockfdo, int level,
                       int optname, const void *optval,
                       socklen_t optlen)
{
	if (level != SOL_SOCKET) {
		Genode::error(__func__, ": Unsupported level ",  level, ", we only support SOL_SOCKET for now");
		errno = EBADF;
		return -1;
	}

	optname = translate_ops_linux(optname);
	if (optname < 0) {
		errno = ENOPROTOOPT;
		return -1;
	}

	return socketcall().setsockopt(context(sockfdo)->handle(), Lxip::LINUX_SOL_SOCKET,
	                             optname, optval, optlen);
}


Libc::File_descriptor *Plugin::socket(int domain, int type, int protocol)
{
	using namespace Lxip;
	Handle handle = socketcall().socket(type == SOCK_STREAM ? TYPE_STREAM : TYPE_DGRAM);

	if (!handle.socket) {
		errno = EBADF;
		return 0;
	}

	Plugin_context *context   = new (heap()) Plugin_context(handle);
	Libc::File_descriptor *fd = Libc::file_descriptor_allocator()->alloc(this, context);
	return fd;
}


ssize_t Plugin::write(Libc::File_descriptor *fdo, const void *buf, ::size_t count)
{
	return send(fdo, buf, count, 0);
}


int Plugin::linux_family(const struct sockaddr *addr, socklen_t addrlen)
{
	switch (addr->sa_family)
	{
		case AF_INET:

				return LINUX_AF_INET;

		default:

				Genode::error("unsupported socket BSD protocol ", addr->sa_family);
			return 0;
	}

	return 0;
}


int Plugin::bsd_family(struct sockaddr *addr)
{
	/*
	 * Note: Since in Linux 'sa_family' is 16 bit while in BSD it is 8 bit (both
	 * little endian), 'sa_len' will contain the actual family (or lower order
	 * bits) of Linux
	 */
	addr->sa_family = addr->sa_len;

	switch (addr->sa_family)
	{
		case LINUX_AF_INET: return AF_INET;

		default:

			Genode::error("unsupported socket Linux protocol ", addr->sa_family);
			return 0;
	}
}


int Plugin::translate_msg_flags(int bsd_flags)
{
	using namespace Lxip;
	int f = 0;

	if (bsd_flags & MSG_OOB)          f |= LINUX_MSG_OOB;
	if (bsd_flags & MSG_PEEK)         f |= LINUX_MSG_PEEK;
	if (bsd_flags & MSG_DONTROUTE)    f |= LINUX_MSG_DONTROUTE;
	if (bsd_flags & MSG_EOR)          f |= LINUX_MSG_EOR;
	if (bsd_flags & MSG_TRUNC)        f |= LINUX_MSG_TRUNC;
	if (bsd_flags & MSG_CTRUNC)       f |= LINUX_MSG_CTRUNC;
	if (bsd_flags & MSG_WAITALL)      f |= LINUX_MSG_WAITALL;
	if (bsd_flags & MSG_NOTIFICATION) Genode::warning("MSG_NOTIFICATION ignored");
	if (bsd_flags & MSG_DONTWAIT)     f |= LINUX_MSG_DONTWAIT;
	if (bsd_flags & MSG_EOF)          f |= LINUX_MSG_EOF;
	if (bsd_flags & MSG_NBIO)         Genode::warning("MSG_NBIO ignored");
	if (bsd_flags & MSG_NOSIGNAL)     f |= LINUX_MSG_NOSIGNAL;
	if (bsd_flags & MSG_COMPAT)       f |= LINUX_MSG_COMPAT;

	return f;
}


/* index is Linux, value is BSD */
static int sockopts[]
{
	 0, /* 0 */
	SO_DEBUG,
	SO_REUSEADDR,
	SO_TYPE,
	SO_ERROR,
	SO_DONTROUTE,          /* 5 */
	SO_BROADCAST,
	SO_SNDBUF,
	SO_RCVBUF,
	SO_KEEPALIVE,
	SO_OOBINLINE,          /* 10 */
	/* SO_NOCHECK */  0,
	/* SO_PRIORITY */ 0,
	SO_LINGER,
	/* SO_BSDCOMPAT */ 0,
	SO_REUSEPORT,          /* 15 */
	/* SO_PASSCRED */ 0,
	/* SO_PEERCRED */ 0,
	SO_RCVLOWAT,
	SO_SNDLOWAT,
	SO_RCVTIMEO,           /* 20 */
	SO_SNDTIMEO,
	0,
	0,
	0,
	0,                     /* 25 */
	0,
	0,
	SO_PEERLABEL,
	SO_TIMESTAMP,
	SO_ACCEPTCONN,         /* 30 */
	-1
};


int Plugin::translate_ops_linux(int optname)
{
	for (int i = 0; sockopts[i] > -1; i++)
		if (sockopts[i] == optname)
			return i;
	
	Genode::error("unsupported sockopt ", optname);
	return -1;
}

} /* unnamed namespace */


void __attribute__((constructor)) init_lxip_plugin()
{
	static Plugin lxip_plugin;
}
