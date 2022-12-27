/*
 * \brief Linux initramfs init binary to load Genode as init process
 * \author Johannes Kliemann
 * \date 2018-04-19
 *
 */

/*
 * Copyright (C) 2018 Genode Labs GmbH
 * Copyright (C) 2018 Componolit GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <sys/mount.h>

int
main(int argc __attribute__((unused)), char *argv[] __attribute__((unused)))
{
	char *args = "core";

	printf("preparing environment for Genode\n");
	if (mount("none", "/dev", "devtmpfs", 0, ""))
		perror("mount");

	close(open("/dev/platform_info", O_RDONLY));

	printf("loading Genode on Linux\n");
	if (chdir("/genode")){
		perror("failed to chdir into /genode");
		return 1;
	}
	if (execve("core", &args, NULL)){
		perror("failed to start core");
		return 1;
	}
}
