/*
 * \brief  Libc includes for the libc_integration test
 * \author Norman Feske
 * \date   2021-12-10
 */

/*
 * Copyright (C) 2021 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _LIBC_H_
#define _LIBC_H_

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconversion"

#include <fcntl.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <sys/select.h>

#pragma GCC diagnostic pop  /* restore -Wconversion warnings */

#endif /* _LIBC_H_ */
