/*
 * \brief  Dummy declarations of Linux-specific libc types and functions
 *         needed to build gdbserver
 * \author Christian Prochaska
 * \date   2011-09-01
 */

/*
 * Copyright (C) 2011-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef SYS_PTRACE_H
#define SYS_PTRACE_H

#include <sys/types.h>

#include <gdbserver_libc_support.h>

enum __ptrace_request {
	PTRACE_TRACEME     = 0,
	PTRACE_PEEKTEXT    = 1,
	PTRACE_PEEKUSER    = 3,
	PTRACE_POKETEXT    = 4,
	PTRACE_POKEUSER    = 6,
	PTRACE_CONT        = 7,
	PTRACE_KILL        = 8,
	PTRACE_SINGLESTEP  = 9,
	PTRACE_GETREGS     = 12,
	PTRACE_SETREGS     = 13,
	PTRACE_ATTACH      = 16,
	PTRACE_DETACH      = 17,

	PTRACE_EVENT_CLONE = 3,

	PTRACE_GETEVENTMSG = 0x4201,
	PTRACE_GETREGSET   = 0x4204,
};

extern long ptrace(enum __ptrace_request request, ...);


#endif /* SYS_PTRACE_H */
