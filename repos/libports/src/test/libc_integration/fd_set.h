/*
 * \brief  wrapper for the libc fd_set.
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

#ifndef INTEGRATION_TEST_FD_SET_H
#define INTEGRATION_TEST_FD_SET_H

/* libc includes */
#include <sys/select.h>

/* stdcxx includes */
#include <vector>
#include <algorithm>

/* local includes */
#include "stdcxx_log.h"


namespace Integration_test
{
	using namespace std;

	struct File_descriptor_set
	{
		private:

			vector<int> _fds    { };
			int         _max_fd { 0 };

		public:

			using Fds_iter = vector<int>::const_iterator;

			void add_fd(int const fd)
			{
				_fds.push_back(fd);
			}

			void remove_fd(int const fd)
			{
				_fds.erase(find(_fds.begin(), _fds.end(), fd));
			}

			int max_fd()    const { return _max_fd; }
			size_t count()  const { return _fds.size(); }

			Fds_iter begin() const { return _fds.cbegin(); }
			Fds_iter end()   const { return _fds.cend(); }

			fd_set fds()
			{
				fd_set set { };

				_max_fd = 0;
				FD_ZERO(&set);

				for (auto fd : _fds) {
					FD_SET(fd, &set);
					_max_fd = max(_max_fd, fd);
				}

				return set;
			}

	};
}

#endif  /* INTEGRATION_TEST_FD_SET_H */
