/*
 * \brief  Interface for registering select handler
 * \author Norman Feske
 * \date   2019-09-18
 */

/*
 * Copyright (C) 2019-2024 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _LIBC__INTERNAL__SELECT_H_
#define _LIBC__INTERNAL__SELECT_H_

/* libc-internal includes */
#include <internal/types.h>

namespace Libc {

	struct Select_handler_base;

	struct Select : Interface
	{
		/**
		 * Schedule select handler that is deblocked by ready fd sets
		 */
		virtual void schedule_select(Select_handler_base &) = 0;

		virtual void deschedule_select() = 0;
	};

}

#endif /* _LIBC__INTERNAL__SELECT_H_ */
