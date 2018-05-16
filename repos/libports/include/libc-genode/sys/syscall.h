/*
 * \brief  Minimal support for FreeBSD-specific syscalls
 * \author Christian Helmuth
 * \date   2018-05-16
 */

/*
 * Copyright (C) 2018 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__LIBC_GENODE__SYS__SYSCALL_H_
#define _INCLUDE__LIBC_GENODE__SYS__SYSCALL_H_

#define SYS_thr_self 432  /* pid_t gettid() */

#endif /* _INCLUDE__LIBC_GENODE__SYS__SYSCALL_H_ */
