/*
 * \brief  Linux-specific debug utilities
 * \author Norman Feske
 * \date   2009-05-16
 */

/*
 * Copyright (C) 2009-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/*
 * With the enabled 'DEBUG' flag, status information can be printed directly
 * via a Linux system call by using the 'raw_write_str' function. This output
 * bypasses the Genode 'LOG' mechanism, which is useful for debugging low-level
 * code such as a libC back-end.
 */
#define DEBUG 1


#if DEBUG
#include <linux_syscalls.h>
#endif /* DEBUG */


/**
 * Write function targeting directly the Linux system call layer and bypassing
 * any Genode code.
 */
extern "C" int raw_write_str(const char *str)
{
#if DEBUG
	unsigned len = 0;
	for (; str[len] != 0; len++);
	lx_syscall(SYS_write, (int)1, str, len);
	return len;
#endif /* DEBUG */
}


/**
 * Debug function waiting until the user presses return
 *
 * This function is there to delay the execution of a back-end function such
 * that we have time to attack the GNU debugger to the running process. Once
 * attached, we can continue execution and use 'gdb' for debugging. In the
 * normal mode of operation, this function is never used.
 */
extern "C" void wait_for_continue(void)
{
#if DEBUG
	char buf[16];
	lx_syscall(SYS_read, (int)0, buf, sizeof(buf));
#endif /* DEBUG */
}


extern "C" int get_pid() { return lx_getpid(); }
