/*
 * \brief  Fd_set utlity
 * \author Stefan Thöni
 * \author Pirmin Duss
 * \date   2021-04-08
 */

/*
 * Copyright (C) 2021 Genode Labs GmbH
 * Copyright (C) 2021 gapfruit AG
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _FD_SET_H_
#define _FD_SET_H_

/* libc includes */
#include <sys/select.h>


namespace Lx_fs {
	class Fd_set;
}


class Lx_fs::Fd_set
{
	private:

		fd_set  _fdset { };
		int     _nfds  { };

	public:

		Fd_set(int fd0)
		{
			FD_ZERO(&_fdset);
			FD_SET(fd0, &_fdset);
			_nfds = fd0 + 1;
		}

		fd_set* fdset()              { return &_fdset; }
		int     nfds()         const { return _nfds; }
		bool    is_set(int fd) const { return FD_ISSET(fd, &_fdset); }
};

#endif /* _FD_SET_H_ */
