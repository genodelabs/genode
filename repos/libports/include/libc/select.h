/*
 * \brief  Support for handling select() in LibC components
 * \author Christian Helmuth
 * \date   2017-02-13
 */

/*
 * Copyright (C) 2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__LIBC__SELECT_H_
#define _INCLUDE__LIBC__SELECT_H_

/* Genode includes */
#include <util/reconstructible.h>

/* libc includes */
#include <sys/select.h> /* for fd_set */


namespace Libc {

	struct Select_handler_base
	{
		int    _nfds { };
		fd_set _readfds { };
		fd_set _writefds { };
		fd_set _exceptfds { };

		/**
		 * Traditional select()
		 *
		 * \returns 0 if no descriptor was ready and schedules for later call
		 *          of select_ready(), or number of currently ready file
		 *          descriptors. The latter case does not schedule
		 *          select_ready().
		 */
		int select(int nfds, fd_set &readfds, fd_set &writefds, fd_set &exceptfds);

		virtual void select_ready(int nready, fd_set const &, fd_set const &, fd_set const &) = 0;

		/* \noapi */
		void dispatch_select();
	};

	template <typename T>
	struct Select_handler : Select_handler_base
	{
		T &obj;
		void (T::*member) (int, fd_set const &, fd_set const &, fd_set const &);

		Select_handler(T &obj,
		               void (T::*member)(int, fd_set const &, fd_set const &, fd_set const &))
		: obj(obj), member(member) { }

		void select_ready(int nready, fd_set const &readfds,
		                  fd_set const &writefds, fd_set const &exceptfds) override
		{
			(obj.*member)(nready, readfds, writefds, exceptfds);
		}
	};
}

#endif /* _INCLUDE__LIBC__SELECT_H_ */
