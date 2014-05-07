/*
 * \brief  GDB stub thread
 * \author Christian Prochaska
 * \date   2011-03-10
 */

/*
 * Copyright (C) 2011-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _GDB_STUB_THREAD_H_
#define _GDB_STUB_THREAD_H_

#include <base/thread.h>

#include "cpu_session_component.h"
#include "dataspace_object.h"
#include "rm_session_component.h"
#include "signal_handler_thread.h"

namespace Gdb_monitor {

	using namespace Genode;

	enum { GDB_STUB_STACK_SIZE = 4*4096 };

	class Gdb_stub_thread : public Thread<GDB_STUB_STACK_SIZE>
	{
		private:

			Cpu_session_component *_cpu_session_component;
			Rm_session_component *_rm_session_component;
			Signal_receiver _exception_signal_receiver;
			Gdb_monitor::Signal_handler_thread _signal_handler_thread;

		public:

			Gdb_stub_thread();
			void entry();

			void set_cpu_session_component(Cpu_session_component *cpu_session_component)
			{
				_cpu_session_component = cpu_session_component;
			}

			void set_rm_session_component(Rm_session_component *rm_session_component)
			{
				_rm_session_component = rm_session_component;
			}

			Cpu_session_component *cpu_session_component() { return _cpu_session_component; }
			Rm_session_component *rm_session_component() { return _rm_session_component; }
			Signal_receiver *exception_signal_receiver() { return &_exception_signal_receiver; }
			int signal_fd() { return _signal_handler_thread.pipe_read_fd(); }
	};
}

#endif /* _GDB_STUB_THREAD_H_ */
