/*
 * \brief  Interface for access to current working directory
 * \author Christian Helmuth
 * \date   2020-07-20
 */

/*
 * Copyright (C) 2020 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _LIBC__INTERNAL__CWD_H_
#define _LIBC__INTERNAL__CWD_H_

/* libc-internal includes */
#include <internal/types.h>
#include <libc-plugin/plugin.h>

namespace Libc {

	/**
	 * Interface to resume all user contexts
	 */
	struct Cwd : Interface
	{
		/**
		 * Provide access to current working directory (modifiable)
		 */
		virtual Absolute_path &cwd() = 0;
	};
}

#endif /* _LIBC__INTERNAL__CWD_H_ */
