/*
 * \brief  Linux-specific types for gdbserver
 * \author Christian Prochaska
 * \date   2016-03-16
 */

/*
 * Copyright (C) 2016-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef SYS_WAIT_H
#define SYS_WAIT_H

#define WNOHANG  1

#define __WCLONE 0x80000000

#define WIFEXITED(status)   (status == 0)
#define WIFSTOPPED(status)  (((status) & 0xff) == 0x7f)
#define WIFSIGNALED(status) (!WIFEXITED(status) && !WIFSTOPPED(status))

#define WEXITSTATUS(status) ((status >> 8) & 0xff)
#define WSTOPSIG(status)    ((status >> 8) & 0xff)
#define WTERMSIG(status)    (status & 0x7f)

#define W_STOPCODE(sig)     ((sig) << 8 | 0x7f)

__BEGIN_DECLS

pid_t waitpid(pid_t pid, int *status, int flags);

__END_DECLS

#endif /* SYS_WAIT_H */
