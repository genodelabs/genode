/*
 * \brief  Signal handler thread
 * \author Christian Prochaska
 * \date   2011-08-15
 */

/*
 * Copyright (C) 2011-2013 Genode Labs GmbH
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

	class Signal_handler_thread : public Thread<2*4096>
	{
		private:

			int              _pipefd[2];
			Signal_receiver *_signal_receiver;

		public:

			Signal_handler_thread(Signal_receiver *receiver);
			void entry();
			int pipe_read_fd() { return _pipefd[0]; }
	};

}

#endif /* _SIGNAL_HANDLER_THREAD_H_ */
