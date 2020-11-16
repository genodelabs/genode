/*
 * \brief  wrapper for libc pipe.
 * \author Pirmin Duss
 * \date   2020-11-11
 */

/*
 * Copyright (C) 2020 Genode Labs GmbH
 * Copyright (C) 2020 gapfruit AG
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef INTEGRATION_TEST_PIPE_H
#define INTEGRATION_TEST_PIPE_H

/* libc includes */
#include <fcntl.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

/* local includes */
#include "stdcxx_log.h"


namespace Integration_test
{
	class Pipe_creation_failed { };
	class Pipe;
}


class Integration_test::Pipe
{
	private:

		int  _pipe_fds[2] { -1, -1 };

	public:

		Pipe()
		{
			if (pipe2(_pipe_fds, O_NONBLOCK) != 0) {
				throw Pipe_creation_failed { };
			}
		}

		~Pipe()
		{
			/* close write fd first */
			close(_pipe_fds[1]);
			close(_pipe_fds[0]);
		}

		int read_fd()  const { return _pipe_fds[0]; }
		int write_fd() const { return _pipe_fds[1]; }
};

#endif /* INTEGRATION_TEST_PIPE_H */
