/*
 * \brief  Interface for resuming the execution of user contexts
 * \author Norman Feske
 * \date   2019-09-18
 */

/*
 * Copyright (C) 2019 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _LIBC__INTERNAL__RESUME_H_
#define _LIBC__INTERNAL__RESUME_H_

/* libc-internal includes */
#include <internal/types.h>

namespace Libc {

	/**
	 * Interface to resume all user contexts
	 */
	struct Resume : Interface
	{
		/**
		 * Resumes the main user context as well as any pthread context
		 */
		virtual void resume_all() = 0;
	};
}

#endif /* _LIBC__INTERNAL__RESUME_H_ */
