/*
 * \brief  Global environment for L4Linux support library
 * \author Stefan Kalkowski
 * \date   2011-03-17
 */

/*
 * Copyright (C) 2011-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _L4LX__ENV_H_
#define _L4LX__ENV_H_

/* L4lx includes */
#include <dataspace.h>
#include <rm.h>
#include <task.h>

namespace L4lx {

	enum {
		THREAD_MAX = (1 << 7),
	};


	class Env
	{
		private:

			Region_manager _rm;
			Dataspace_tree _dataspaces;
			Task_tree      _tasks;

			Env();

		public:

			static Env     *env();

			Region_manager *rm()         { return &_rm;         }
			Dataspace_tree *dataspaces() { return &_dataspaces; };
			Task_tree      *tasks()      { return &_tasks;      };
	};
}

#endif /* _L4LX__ENV_H_ */
