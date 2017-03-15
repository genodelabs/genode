/*
 * \brief  Unix emulation environment for Genode
 * \author Josef Soentgen
 * \date   2012-04-13
 */

/*
 * Copyright (C) 2011-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */


/* Genode includes */
#include <dataspace/client.h>
#include <base/lock.h>

#include <lwip/genode.h>

/* Noux includes */
#include <child.h>
#include <socket_io_channel.h>
#include <shared_pointer.h>
#include <io_receptor_registry.h>

/* Libc includes */
#include <sys/select.h>
#include <sys/time.h>
#include <netdb.h>

using namespace Noux;

void (*libc_select_notify)();

/* helper macro for casting the backend */
#define GET_SOCKET_IO_CHANNEL_BACKEND(backend, name) \
	Socket_io_channel_backend *name = \
	dynamic_cast<Socket_io_channel_backend*>(backend)

/**
 * This callback function is called from lwip via the libc_select_notify
 * function pointer if an event occurs.
 */
static void select_notify()
{
	static Genode::Lock mutex;

	/*
	 * The function could be called multiple times while actually
	 * still running.
	 */
	Genode::Lock::Guard guard(mutex);

	for (Io_receptor *r = io_receptor_registry()->first();
	     r != 0; r = r->next()) {
		r->check_and_wakeup();
	}
}


/**
 * Initialise the network subsystem by directly using lwip
 */

void init_network()
{
	log("--- noux: initialize network ---");

	if (!libc_select_notify)
		libc_select_notify = select_notify;
}


/*********************************
 ** Noux net syscall dispatcher **
 *********************************/

bool Noux::Child::_syscall_net(Noux::Session::Syscall sc)
{
	switch (sc) {
		/**
		 * Keep compiler from complaining
		 */
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
		case SYSCALL_READLINK:
		case SYSCALL_SYMLINK:
		case SYSCALL_USERINFO:
		case SYSCALL_GETTIMEOFDAY:
		case SYSCALL_CLOCK_GETTIME:
		case SYSCALL_UTIMES:
		case SYSCALL_SYNC:
		case SYSCALL_KILL:
		case SYSCALL_GETDTABLESIZE:
			break;
		case SYSCALL_SOCKET:
			{
				Socket_io_channel *socket_io_channel = new (_heap) Socket_io_channel();

				GET_SOCKET_IO_CHANNEL_BACKEND(socket_io_channel->backend(), backend);

				if (!backend->socket(_sysio)) {
					delete socket_io_channel;
					return false;
				}

				Shared_pointer<Io_channel> io_channel(socket_io_channel, _heap);

				_sysio.socket_out.fd = add_io_channel(io_channel);

				return true;
			}
		case SYSCALL_GETSOCKOPT:
			{
				Shared_pointer<Io_channel> io = _lookup_channel(_sysio.getsockopt_in.fd);

				GET_SOCKET_IO_CHANNEL_BACKEND(io->backend(), backend);

				return backend->getsockopt(_sysio);
			}
		case SYSCALL_SETSOCKOPT:
			{
				Shared_pointer<Io_channel> io = _lookup_channel(_sysio.setsockopt_in.fd);

				GET_SOCKET_IO_CHANNEL_BACKEND(io->backend(), backend);

				return backend->setsockopt(_sysio);
			}
		case SYSCALL_ACCEPT:
			{
				Shared_pointer<Io_channel> io = _lookup_channel(_sysio.accept_in.fd);

				GET_SOCKET_IO_CHANNEL_BACKEND(io->backend(), backend);

				int socket = backend->accept(_sysio);
				if (socket == -1)
					return false;

				Socket_io_channel *socket_io_channel = new (_heap) Socket_io_channel(socket);
				Shared_pointer<Io_channel> io_channel(socket_io_channel, _heap);

				_sysio.accept_out.fd = add_io_channel(io_channel);

				return true;
			}
		case SYSCALL_BIND:
			{
				Shared_pointer<Io_channel> io = _lookup_channel(_sysio.bind_in.fd);

				GET_SOCKET_IO_CHANNEL_BACKEND(io->backend(), backend);

				return (backend->bind(_sysio) == -1) ? false : true;
			}
		case SYSCALL_LISTEN:
			{
				Shared_pointer<Io_channel> io = _lookup_channel(_sysio.listen_in.fd);

				GET_SOCKET_IO_CHANNEL_BACKEND(io->backend(), backend);

				return (backend->listen(_sysio) == -1) ? false : true;
			}
		case SYSCALL_SEND:
			{
				Shared_pointer<Io_channel> io = _lookup_channel(_sysio.send_in.fd);

				GET_SOCKET_IO_CHANNEL_BACKEND(io->backend(), backend);

				return (backend->send(_sysio) == -1) ? false : true;
			}
		case SYSCALL_SENDTO:
			{
				Shared_pointer<Io_channel> io = _lookup_channel(_sysio.sendto_in.fd);

				GET_SOCKET_IO_CHANNEL_BACKEND(io->backend(), backend);

				return (backend->sendto(_sysio) == -1) ? false : true;
			}
		case SYSCALL_RECV:
			{
				Shared_pointer<Io_channel> io = _lookup_channel(_sysio.recv_in.fd);

				GET_SOCKET_IO_CHANNEL_BACKEND(io->backend(), backend);

				return (backend->recv(_sysio) == -1) ? false : true;
			}
		case SYSCALL_RECVFROM:
			{
				Shared_pointer<Io_channel> io = _lookup_channel(_sysio.recvfrom_in.fd);

				GET_SOCKET_IO_CHANNEL_BACKEND(io->backend(), backend);

				return (backend->recvfrom(_sysio) == -1) ? false : true;
			}
		case SYSCALL_GETPEERNAME:
			{
				Shared_pointer<Io_channel> io = _lookup_channel(_sysio.getpeername_in.fd);

				GET_SOCKET_IO_CHANNEL_BACKEND(io->backend(), backend);

				return (backend->getpeername(_sysio) == -1) ? false : true;
			}
		case SYSCALL_SHUTDOWN:
			{
				Shared_pointer<Io_channel> io = _lookup_channel(_sysio.shutdown_in.fd);

				GET_SOCKET_IO_CHANNEL_BACKEND(io->backend(), backend);

				return (backend->shutdown(_sysio) == -1) ? false : true;
			}
		case SYSCALL_CONNECT:
			{
				Shared_pointer<Io_channel> io = _lookup_channel(_sysio.connect_in.fd);

				GET_SOCKET_IO_CHANNEL_BACKEND(io->backend(), backend);

				return (backend->connect(_sysio) == -1) ? false : true;
			}
	}

	return false;
}
