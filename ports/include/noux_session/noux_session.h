/*
 * \brief  Noux session interface
 * \author Norman Feske
 * \date   2011-02-15
 */

/*
 * Copyright (C) 2011-2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__NOUX_SESSION__NOUX_SESSION_H_
#define _INCLUDE__NOUX_SESSION__NOUX_SESSION_H_

#include <base/stdint.h>
#include <session/session.h>
#include <dataspace/capability.h>

#define NOUX_DECL_SYSCALL_NAME(name) \
	case SYSCALL_##name:  return #name;

namespace Noux {

	using namespace Genode;

	struct Session : Genode::Session
	{
		static const char *service_name() { return "Noux"; }

		virtual ~Session() { }

		virtual Dataspace_capability sysio_dataspace() = 0;

		enum Syscall {
			SYSCALL_GETCWD,
			SYSCALL_WRITE,
			SYSCALL_READ,
			SYSCALL_STAT,
			SYSCALL_LSTAT,
			SYSCALL_FSTAT,
			SYSCALL_FCNTL,
			SYSCALL_OPEN,
			SYSCALL_CLOSE,
			SYSCALL_IOCTL,
			SYSCALL_LSEEK,
			SYSCALL_DIRENT,
			SYSCALL_FCHDIR,
			SYSCALL_EXECVE,
			SYSCALL_SELECT,
			SYSCALL_FORK,
			SYSCALL_GETPID,
			SYSCALL_WAIT4,
			SYSCALL_PIPE,
			SYSCALL_DUP2,
			SYSCALL_UNLINK,
			SYSCALL_RENAME,
			SYSCALL_MKDIR,
			SYSCALL_SOCKET,
			SYSCALL_GETSOCKOPT,
			SYSCALL_SETSOCKOPT,
			SYSCALL_ACCEPT,
			SYSCALL_BIND,
			SYSCALL_LISTEN,
			SYSCALL_SEND,
			SYSCALL_SENDTO,
			SYSCALL_RECV,
			SYSCALL_GETPEERNAME,
			SYSCALL_SHUTDOWN,
			SYSCALL_CONNECT,
			SYSCALL_GETADDRINFO,
			SYSCALL_INVALID = -1
		};

		static char const *syscall_name(Syscall sc)
		{
			switch (sc) {
			NOUX_DECL_SYSCALL_NAME(GETCWD)
			NOUX_DECL_SYSCALL_NAME(WRITE)
			NOUX_DECL_SYSCALL_NAME(READ)
			NOUX_DECL_SYSCALL_NAME(STAT)
			NOUX_DECL_SYSCALL_NAME(LSTAT)
			NOUX_DECL_SYSCALL_NAME(FSTAT)
			NOUX_DECL_SYSCALL_NAME(FCNTL)
			NOUX_DECL_SYSCALL_NAME(OPEN)
			NOUX_DECL_SYSCALL_NAME(CLOSE)
			NOUX_DECL_SYSCALL_NAME(IOCTL)
			NOUX_DECL_SYSCALL_NAME(LSEEK)
			NOUX_DECL_SYSCALL_NAME(DIRENT)
			NOUX_DECL_SYSCALL_NAME(FCHDIR)
			NOUX_DECL_SYSCALL_NAME(EXECVE)
			NOUX_DECL_SYSCALL_NAME(SELECT)
			NOUX_DECL_SYSCALL_NAME(FORK)
			NOUX_DECL_SYSCALL_NAME(GETPID)
			NOUX_DECL_SYSCALL_NAME(WAIT4)
			NOUX_DECL_SYSCALL_NAME(PIPE)
			NOUX_DECL_SYSCALL_NAME(DUP2)
			NOUX_DECL_SYSCALL_NAME(UNLINK)
			NOUX_DECL_SYSCALL_NAME(RENAME)
			NOUX_DECL_SYSCALL_NAME(MKDIR)
			NOUX_DECL_SYSCALL_NAME(SOCKET)
			NOUX_DECL_SYSCALL_NAME(GETSOCKOPT)
			NOUX_DECL_SYSCALL_NAME(SETSOCKOPT)
			NOUX_DECL_SYSCALL_NAME(ACCEPT)
			NOUX_DECL_SYSCALL_NAME(BIND)
			NOUX_DECL_SYSCALL_NAME(LISTEN)
			NOUX_DECL_SYSCALL_NAME(SEND)
			NOUX_DECL_SYSCALL_NAME(SENDTO)
			NOUX_DECL_SYSCALL_NAME(RECV)
			NOUX_DECL_SYSCALL_NAME(GETPEERNAME)
			NOUX_DECL_SYSCALL_NAME(SHUTDOWN)
			NOUX_DECL_SYSCALL_NAME(CONNECT)
			NOUX_DECL_SYSCALL_NAME(GETADDRINFO)
			case SYSCALL_INVALID: return 0;
			}
			return 0;
		}

		/**
		 * Perform syscall
		 *
		 * The syscall arguments and results are communicated via the shared
		 * sysio dataspace.
		 *
		 * \return true on success
		 */
		virtual bool syscall(Syscall syscall) = 0;


		/*********************
		 ** RPC declaration **
		 *********************/

		GENODE_RPC(Rpc_sysio_dataspace, Dataspace_capability, sysio_dataspace);
		GENODE_RPC(Rpc_syscall, bool, syscall, Syscall);

		GENODE_RPC_INTERFACE(Rpc_sysio_dataspace, Rpc_syscall);
	};
}

#undef NOUX_DECL_SYSCALL_NAME

#endif /* _INCLUDE__NOUX_SESSION__NOUX_SESSION_H_ */

