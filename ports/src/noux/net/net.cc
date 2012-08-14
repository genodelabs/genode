/*
 * \brief  Unix emulation environment for Genode
 * \author Josef Soentgen
 * \date   2012-04-13
 */

/*
 * Copyright (C) 2011-2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */


/* Genode includes */
#include <cap_session/connection.h>
#include <dataspace/client.h>

#include <lwip/genode.h>

/* Noux includes */
#include <child.h>
#include <socket_descriptor_registry.h>
#include <socket_io_channel.h>
#include <shared_pointer.h>

/* Libc includes */
#include <sys/select.h>
#include <sys/time.h>
#include <netdb.h>

using namespace Noux;

void (*libc_select_notify)();
void (*close_socket)(int);

/* set select() timeout to lwip's lowest possible value */
struct timeval timeout = { 0, 10000 };


/**
 * This callback function is called from lwip via the libc_select_notify
 * function pointer if an event occurs.
 */

static void select_notify()
{
	fd_set readfds;
	fd_set writefds;
	fd_set exceptfds;
	int ready;

	FD_ZERO(&readfds);
	FD_ZERO(&writefds);
	FD_ZERO(&exceptfds);

	/* for now set each currently used socket descriptor true */
	for (int sd = 0; sd < MAX_SOCKET_DESCRIPTORS; sd++) {
		if (Socket_descriptor_registry<Socket_io_channel>::instance()->sd_in_use(sd)) {
			int real_sd = Socket_descriptor_registry<Socket_io_channel>::instance()->io_channel_by_sd(sd)->get_socket();

			FD_SET(real_sd, &readfds);
			FD_SET(real_sd, &writefds);
			FD_SET(real_sd, &exceptfds);
		}
	}

	ready = ::select(MAX_SOCKET_DESCRIPTORS, &readfds, &writefds, &exceptfds, &timeout);

	/* if any socket is ready for reading */
	if (ready > 0) {
		for (int sd = 0; sd < MAX_SOCKET_DESCRIPTORS; sd++) {
			if (Socket_descriptor_registry<Socket_io_channel>::instance()->sd_in_use(sd)) {
				int real_sd = Socket_descriptor_registry<Socket_io_channel>::instance()->io_channel_by_sd(sd)->get_socket();

				if (FD_ISSET(real_sd, &readfds)
					|| FD_ISSET(real_sd, &writefds)
					|| FD_ISSET(real_sd, &exceptfds)) {
					Shared_pointer<Socket_io_channel> sio = Socket_descriptor_registry<Socket_io_channel>::instance()->io_channel_by_sd(sd);

					if (FD_ISSET(real_sd, &readfds))
						sio->set_unblock(true, false, false);
					if (FD_ISSET(real_sd, &writefds))
						sio->set_unblock(false, true, false);
					if (FD_ISSET(real_sd, &exceptfds))
						sio->set_unblock(false, false, true);

					sio->invoke_all_notifiers();
				}
			}
		}
	}
}


static void _close_socket(int sd)
{
	Socket_descriptor_registry<Socket_io_channel>::instance()->remove_io_channel(sd);
}


/**
 * Initialise the network subsystem by directly using lwip
 */

void init_network()
{
	PINF("--- noux: initialize network ---");
	
	/**
	 * NOTE: we only call lwip_nic_init() because
	 * lwip_tcpip_init() was already called by libc_lwip's
	 * constructor and we don't want to have another tcpip
	 * thread.
	 */

	lwip_nic_init(0, 0, 0);

	if (!libc_select_notify)
		libc_select_notify = select_notify;

	if (!close_socket)
		close_socket = _close_socket;
}

/*********************************
 ** Noux net syscall dispatcher **
 *********************************/

#define GET_SOCKET_IO_CHANNEL(fd, handle)                      \
	Shared_pointer<Io_channel> io = _lookup_channel(fd);   \
	Shared_pointer<Socket_io_channel> handle =             \
		io.dynamic_pointer_cast<Socket_io_channel>();

bool Noux::Child::_syscall_net(Noux::Session::Syscall sc)
{
	switch (sc) {
		/**
		 * Keep compiler from complaining
		 */
		case SYSCALL_GETCWD:
		case SYSCALL_WRITE:
		case SYSCALL_READ:
		case SYSCALL_STAT:
		case SYSCALL_LSTAT:
		case SYSCALL_FSTAT:
		case SYSCALL_FCNTL:
		case SYSCALL_OPEN:
		case SYSCALL_CLOSE:
		case SYSCALL_IOCTL:
		case SYSCALL_LSEEK:
		case SYSCALL_DIRENT:
		case SYSCALL_FCHDIR:
		case SYSCALL_EXECVE:
		case SYSCALL_SELECT:
		case SYSCALL_FORK:
		case SYSCALL_GETPID:
		case SYSCALL_WAIT4:
		case SYSCALL_PIPE:
		case SYSCALL_DUP2:
		case SYSCALL_INVALID:
		case SYSCALL_UNLINK:
		case SYSCALL_RENAME:
		case SYSCALL_MKDIR:
		case SYSCALL_FTRUNCATE:
			break;
		case SYSCALL_SOCKET:
			{
				Socket_io_channel *socket_io_channel = new Socket_io_channel();

				if (!socket_io_channel->socket(_sysio)) {
					delete socket_io_channel;
					return false;
				}

				Shared_pointer<Io_channel> io_channel(socket_io_channel, Genode::env()->heap());

				_sysio->socket_out.fd = add_io_channel(io_channel);

				/* add socket to registry */
				Socket_descriptor_registry<Socket_io_channel>::instance()->add_io_channel(io_channel.dynamic_pointer_cast<Socket_io_channel>(),
						_sysio->socket_out.fd);

				return true;
			}
		case SYSCALL_GETSOCKOPT:
			{
				GET_SOCKET_IO_CHANNEL(_sysio->getsockopt_in.fd, socket_io_channel)

				if (!socket_io_channel->getsockopt(_sysio))
					return false;

				return true;
			}
		case SYSCALL_SETSOCKOPT:
			{
				GET_SOCKET_IO_CHANNEL(_sysio->setsockopt_in.fd, socket_io_channel)

				if (!socket_io_channel->setsockopt(_sysio)) {
					return false;
				}

				return true;
			}
		case SYSCALL_ACCEPT:
			{
				GET_SOCKET_IO_CHANNEL(_sysio->accept_in.fd, socket_io_channel)

				int new_socket = socket_io_channel->accept(_sysio);
				if (new_socket == -1)
					return false;

				Socket_io_channel *new_socket_io_channel = new Socket_io_channel(new_socket);
				Shared_pointer<Io_channel> channel(new_socket_io_channel, Genode::env()->heap());

				_sysio->accept_out.fd = add_io_channel(channel);

				/* add new socket to registry */
				Socket_descriptor_registry<Socket_io_channel>::instance()->add_io_channel(channel.dynamic_pointer_cast<Socket_io_channel>(),
					_sysio->accept_out.fd);

				return true;
			}
		case SYSCALL_BIND:
			{
				GET_SOCKET_IO_CHANNEL(_sysio->bind_in.fd, socket_io_channel)

				if (socket_io_channel->bind(_sysio) == -1)
					return false;

				return true;
			}
		case SYSCALL_LISTEN:
			{
				GET_SOCKET_IO_CHANNEL(_sysio->listen_in.fd, socket_io_channel)

				if (socket_io_channel->listen(_sysio) == -1)
					return false;

				return true;
			}
		case SYSCALL_SEND:
			{
				GET_SOCKET_IO_CHANNEL(_sysio->send_in.fd, socket_io_channel)

				ssize_t len = socket_io_channel->send(_sysio);

				if (len == -1)
					return false;

				_sysio->send_out.len = len;

				return true;
			}
		case SYSCALL_SENDTO:
			{
				GET_SOCKET_IO_CHANNEL(_sysio->sendto_in.fd, socket_io_channel)

				ssize_t len = socket_io_channel->sendto(_sysio);

				if (len == -1)
					return false;

				_sysio->sendto_out.len = len;

				return true;
			}
		case SYSCALL_RECV:
			{
				GET_SOCKET_IO_CHANNEL(_sysio->recv_in.fd, socket_io_channel)

				ssize_t len = socket_io_channel->recv(_sysio);

				if (len == -1)
					return false;

				_sysio->recv_out.len = len;

				return true;
			}
		case SYSCALL_RECVFROM:
			{
				GET_SOCKET_IO_CHANNEL(_sysio->recvfrom_in.fd, socket_io_channel)

				ssize_t len = socket_io_channel->recvfrom(_sysio);

				if (len == -1)
					return false;

				_sysio->recvfrom_out.len = len;

				return true;
			}
		case SYSCALL_GETPEERNAME:
			{
				GET_SOCKET_IO_CHANNEL(_sysio->getpeername_in.fd, socket_io_channel)

				int res = socket_io_channel->getpeername(_sysio);

				if (res == -1)
					return false;

				return true;
			}
		case SYSCALL_SHUTDOWN:
			{
				GET_SOCKET_IO_CHANNEL(_sysio->shutdown_in.fd, socket_io_channel)

				int res = socket_io_channel->shutdown(_sysio);

				if (res == -1)
					return false;

				/* remove sd from registry */
				_close_socket(_sysio->shutdown_in.fd);

				return true;
			}
		case SYSCALL_CONNECT:
			{
				GET_SOCKET_IO_CHANNEL(_sysio->connect_in.fd, socket_io_channel);

				int res = socket_io_channel->connect(_sysio);

				if (res == -1)
					return false;

				return true;
			}
		case SYSCALL_GETADDRINFO:
			{
#if 0
				struct addrinfo *result, *rp = NULL;

				int res = lwip_getaddrinfo(_sysio->getaddrinfo_in.hostname,
							_sysio->getaddrinfo_in.servname,
							(const struct addrinfo *)&_sysio->getaddrinfo_in.hints,
							&result);

				if (res != 0) {
					PERR("::getaddrinfo() returns %d", res);
					return false;
				}

				PINF("SYSCALL_GETADDRINFO: deep-copy");
				/* wipe-out old state */
				memset(_sysio->getaddrinfo_in.res, 0, sizeof (_sysio->getaddrinfo_in.res));

				int i = 0; rp = result;
				while (i < Noux::Sysio::MAX_ADDRINFO_RESULTS && rp != NULL) {
					memcpy(&_sysio->getaddrinfo_in.res[i].addrinfo, rp, sizeof (struct addrinfo));
					if (rp->ai_addr) {
						memcpy(&_sysio->getaddrinfo_in.res[i].ai_addr, rp->ai_addr, sizeof (struct sockaddr));
					}
					else
						memset(&_sysio->getaddrinfo_in.res[i].ai_addr, 0, sizeof (struct sockaddr));

					if (rp->ai_canonname) {
						memcpy(&_sysio->getaddrinfo_in.res[i].ai_canonname,
								rp->ai_canonname, strlen(rp->ai_canonname));
						PINF("kopiere canonname: '%s'", rp->ai_canonname);
					}
					else
						memset(&_sysio->getaddrinfo_in.res[i].ai_canonname, 0,
								sizeof (_sysio->getaddrinfo_in.res[i].ai_canonname));

					i++; rp = rp->ai_next;
				}
				
				_sysio->getaddrinfo_out.addr_num = i;
				PINF("SYSCALL_GETADDRINFO: deep-copy successfull");

				lwip_freeaddrinfo(result);

				return true;
#endif
				return false;
			}
	}

	return false;
}
