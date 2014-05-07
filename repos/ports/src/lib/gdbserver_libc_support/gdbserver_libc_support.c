/*
 * \brief  Dummy implementations of Linux-specific libc functions
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

#include <errno.h>
#include <stdio.h>

#include <sys/ptrace.h>

long ptrace(enum __ptrace_request request, ...)
{
	char *request_str = 0;

	switch (request) {
		case PTRACE_TRACEME:    request_str = "PTRACE_TRACEME";    break;
		case PTRACE_ATTACH:     request_str = "PTRACE_ATTACH";     break;
		case PTRACE_KILL:       request_str = "PTRACE_KILL";       break;
		case PTRACE_DETACH:     request_str = "PTRACE_DETACH";     break;
		case PTRACE_SINGLESTEP: request_str = "PTRACE_SINGLESTEP"; break;
		case PTRACE_CONT:       request_str = "PTRACE_CONT";       break;
		case PTRACE_PEEKTEXT:   request_str = "PTRACE_PEEKTEXT";   break;
		case PTRACE_POKETEXT:   request_str = "PTRACE_POKETEXT";   break;
		case PTRACE_PEEKUSER:   request_str = "PTRACE_PEEKUSER";   break;
		case PTRACE_POKEUSER:   request_str = "PTRACE_POKEUSER";   break;
		case PTRACE_GETREGS:    request_str = "PTRACE_GETREGS";    break;
		case PTRACE_SETREGS:    request_str = "PTRACE_SETREGS";    break;
	}

	printf("ptrace(%s) called - not implemented!\n", request_str);

	errno = EINVAL;
	return -1;
}
