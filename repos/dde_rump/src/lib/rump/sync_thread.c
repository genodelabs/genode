/*
 * \brief  Rump kernel thread which syncs the file system every 10s
 * \author Christian Prochaska
 * \date   2022-04-08
 */

/*
 * Copyright (C) 2022 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <sys/cpu.h>
#include <sys/param.h>
#include <sys/vfs_syscalls.h>
#include <rump/rumpuser.h>

extern void rump_io_backend_sync();

void genode_sync_thread(void *arg)
{
	for (;;) {
		/* sleep for 10 seconds */
		rumpuser_clock_sleep(RUMPUSER_CLOCK_RELWALL, 10, 0);

		/* sync through front-end */
		do_sys_sync(curlwp);

		/* sync Genode back-end */
		rump_io_backend_sync();
	}
}
