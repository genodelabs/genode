/*
 * \brief  Signal handler thread
 * \author Christian Prochaska
 * \date   2011-08-15
 */

/*
 * Copyright (C) 2011-2016 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _SIGNAL_HANDLER_THREAD_H_
#define _SIGNAL_HANDLER_THREAD_H_

#include <base/signal.h>
#include <base/thread.h>

using namespace Genode;

namespace Gdb_monitor {

	enum { SIGNAL_HANDLER_THREAD_STACK_SIZE = 2*1024*sizeof(addr_t) };

	class Signal_handler_thread
	: public Thread_deprecated<SIGNAL_HANDLER_THREAD_STACK_SIZE>
	{
		private:

			Signal_receiver *_signal_receiver;

		public:

			Signal_handler_thread(Signal_receiver *receiver);
			void entry();
	};

}

#endif /* _SIGNAL_HANDLER_THREAD_H_ */
