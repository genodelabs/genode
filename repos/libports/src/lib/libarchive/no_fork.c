/*
 * \brief  Dummy stubs to satisfy the linking of libarchive
 * \author Norman Feske
 * \date   2017-09-26
 */

/*
 * Copyright (C) 2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>

pid_t __archive_create_child(const char *cmd, int *stdin, int *stdout)
{
	printf("libarchive: __archive_create_child called but not implemented\n");
	abort();
}

void __archive_check_child(int in, int out)
{
	printf("libarchive: __archive_check_child called but not implemented\n");
	abort();
}

