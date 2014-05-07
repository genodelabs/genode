/*
 * \brief  Dummy declarations of Linux-specific libc types and functions
 *         needed to build gdbserver
 * \author Christian Prochaska
 * \date   2011-09-01
 */

/*
 * Copyright (C) 2011-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef SYS_PTRACE_H
#define SYS_PTRACE_H

#include <sys/types.h>

#include <gdbserver_libc_support.h>

enum __ptrace_request {
	PTRACE_TRACEME,
	PTRACE_ATTACH,
	PTRACE_KILL,
	PTRACE_DETACH,
	PTRACE_SINGLESTEP,
	PTRACE_CONT,
	PTRACE_PEEKTEXT,
	PTRACE_POKETEXT,
	PTRACE_PEEKUSER,
	PTRACE_POKEUSER,
	PTRACE_GETREGS,
	PTRACE_SETREGS,
};

extern long ptrace(enum __ptrace_request request, ...);


#endif /* SYS_PTRACE_H */
