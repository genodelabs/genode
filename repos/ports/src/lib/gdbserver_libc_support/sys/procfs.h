/*
 * \brief  Dummy declarations of Linux-specific libc types and macros
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

#ifndef SYS_PROCFS_H
#define SYS_PROCFS_H

typedef enum {
	PS_OK,
	PS_ERR,
} ps_err_e;

struct ps_prochandle { };

#endif /* SYS_PROCFS_H */
