/*
 * \brief  Thread info class for GDB monitor
 * \author Christian Prochaska
 * \date   2011-09-09
 */

/*
 * Copyright (C) 2011-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _THREAD_INFO_H_
#define _THREAD_INFO_H_

#include <base/thread.h>

#include "append_list.h"

namespace Gdb_monitor {

	using namespace Genode;

	class Thread_info : public Signal_context, public Append_list<Thread_info>::Element
	{
		private:

			Thread_capability _thread_cap;
			unsigned long _lwpid;

		public:

			Thread_info(Thread_capability thread_cap, unsigned long lwpid)
			: _thread_cap(thread_cap),
			  _lwpid(lwpid) { }

			Thread_capability thread_cap() { return _thread_cap; }
			unsigned long lwpid() { return _lwpid; }
	};
}

#endif /* _THREAD_INFO_H_ */
