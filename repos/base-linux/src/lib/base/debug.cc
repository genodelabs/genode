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

#include <linux_syscalls.h>

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
	char buf[16];
	lx_syscall(SYS_read, (int)0, buf, sizeof(buf));
}
