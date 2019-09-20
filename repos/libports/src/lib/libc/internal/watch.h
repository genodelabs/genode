/*
 * \brief  Interface for watching files
 * \author Norman Feske
 * \date   2019-09-20
 */

/*
 * Copyright (C) 2019 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _LIBC__INTERNAL__WATCH_H_
#define _LIBC__INTERNAL__WATCH_H_

/* libc-internal includes */
#include <internal/types.h>

namespace Libc {

	struct Watch : Interface
	{
		/**
		 * Alloc new watch handler for given path
		 */
		virtual Vfs::Vfs_watch_handle *alloc_watch_handle(char const *path) = 0;
	};
}

#endif /* _LIBC__INTERNAL__WATCH_H_ */
