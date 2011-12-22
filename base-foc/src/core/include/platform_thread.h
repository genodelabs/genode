/*
 * \brief   Fiasco thread facility
 * \author  Christian Helmuth
 * \author  Stefan Kalkowski
 * \date    2006-04-11
 */

/*
 * Copyright (C) 2006-2011 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _CORE__INCLUDE__PLATFORM_THREAD_H_
#define _CORE__INCLUDE__PLATFORM_THREAD_H_

/* Genode includes */
#include <base/native_types.h>
#include <base/thread_state.h>
#include <base/pager.h>

/* core includes */
#include <platform_pd.h>
#include <cap_session_component.h>

/* Fiasco includes */
namespace Fiasco {
#include <l4/sys/types.h>
}

namespace Genode {

	class Platform_pd;
	class Platform_thread
	{
		private:

			friend class Platform_pd;

			bool              _core_thread;
			Native_capability _thread_cap;
			Native_capability _gate_cap;
			Native_capability _remote_gate_cap;
			Native_thread     _remote_pager_cap;
			Native_thread     _irq_cap;
			Native_thread     _remote_irq_cap;
			Capability_node   _node;
			Native_utcb       _utcb;
			char              _name[32];       /* thread name that will be
			                                     registered at the kernel
			                                     debugger */
			Platform_pd      *_platform_pd;    /* protection domain thread
			                                     is bound to */
			Pager_object     *_pager;

			void _create_thread(void);
			void _finalize_construction(const char *name, unsigned prio);
			bool _in_syscall(Fiasco::l4_umword_t flags);

		public:

			enum { DEFAULT_PRIORITY = 128 };

			/**
			 * Constructor for non-core threads
			 */
			Platform_thread(const char *name, unsigned priority);

			/**
			 * Constructor for core main-thread
			 */
			Platform_thread(Native_thread cap, const char *name);

			/**
			 * Constructor for core threads
			 */
			Platform_thread(const char *name);

			/**
			 * Destructor
			 */
			~Platform_thread();

			/**
			 * Start thread
			 *
			 * \param ip  instruction pointer to start at
			 * \param sp  stack pointer to use
			 *
			 * \retval  0  successful
			 * \retval -1  thread could not be started
			 */
			int start(void *ip, void *sp);

			/**
			 * Pause this thread
			 */
			void pause();

			/**
			 * Resume this thread
			 */
			void resume();

			/**
			 * Cancel currently blocking operation
			 */
			void cancel_blocking();

			/**
			 * This thread is about to be bound
			 *
			 * \param cap   final capability index
			 * \param pd    platform pd, thread is bound to
			 */
			void bind(/*Native_thread_id cap, */Platform_pd *pd);

			/**
			 * Unbind this thread
			 */
			void unbind();

			/**
			 * Request thread state
			 *
			 * \param  state_dst  destination state buffer
			 *
			 * \retval  0 successful
			 * \retval -1 thread state not accessible
			 */
			int state(Genode::Thread_state *state_dst);


			/************************
			 ** Accessor functions **
			 ************************/

			/**
			 * Return/set pager
			 */
			Pager_object *pager() const { return _pager; }
			void pager(Pager_object *pager);

			/**
			 * Return identification of thread when faulting
			 */
			unsigned long pager_object_badge() {
				return (unsigned long) _thread_cap.dst(); }


			/*******************************
			 ** Fiasco-specific Accessors **
			 *******************************/

			Native_thread     native_thread() const { return _thread_cap.dst(); }
			Native_capability thread_cap()    const { return _thread_cap; }
			Native_capability gate()          const { return _remote_gate_cap; }
			const char       *name()          const { return _name; }
			bool              core_thread()   const { return _core_thread; }
	};
}

#endif /* _CORE__INCLUDE__PLATFORM_THREAD_H_ */
