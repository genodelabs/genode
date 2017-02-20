/*
 * \brief  Dummy declarations of Linux-specific libc types and macros
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

#ifndef GDBSERVER_LIBC_DUMMIES_H
#define GDBSERVER_LIBC_DUMMIES_H

/* Missing in libc's elf.h */

#define AT_HWCAP 16 /* Machine dependent hints about
					   processor capabilities.  */

/* Missing in libc's sys/signal.h */

struct siginfo { };

/* sys/user.h */
struct user {
		unsigned long int u_debugreg [8];
};

#endif /* GDBSERVER_LIBC_DUMMIES_H */
