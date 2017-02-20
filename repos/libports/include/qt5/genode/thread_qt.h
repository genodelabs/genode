/*
 * \brief  Thread with preconfigured stack size
 * \author Christian Prochaska
 * \date   2008-06-11
 */

/*
 * Copyright (C) 2008-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__BASE__THREAD_QT_H_
#define _INCLUDE__BASE__THREAD_QT_H_

#include <base/thread.h>

enum { DEFAULT_STACK_SIZE = 4096*100 };

namespace Genode {

	class Thread_qt : public Thread_deprecated<DEFAULT_STACK_SIZE>
	{
		public:

			explicit Thread_qt(const char *name = "Qt <noname>")
			: Thread_deprecated<DEFAULT_STACK_SIZE>(name) { }
	};
}

#endif /* _INCLUDE__BASE__THREAD_QT_H_ */
