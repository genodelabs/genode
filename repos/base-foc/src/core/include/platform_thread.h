/*
 * \brief   Fiasco thread facility
 * \author  Christian Helmuth
 * \author  Stefan Kalkowski
 * \date    2006-04-11
 */

/*
 * Copyright (C) 2006-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _CORE__INCLUDE__PLATFORM_THREAD_H_
#define _CORE__INCLUDE__PLATFORM_THREAD_H_

/* Genode includes */
#include <base/native_capability.h>
#include <base/thread_state.h>

/* core includes */
#include <pager.h>
#include <platform_pd.h>
#include <cap_mapping.h>

namespace Genode {

	class Platform_pd;
	class Platform_thread
	{
		private:

			enum State { DEAD, RUNNING };

			friend class Platform_pd;

			State              _state;
			bool               _core_thread;
			Cap_mapping        _thread;
			Cap_mapping        _gate;
			Cap_mapping        _pager;
			Cap_mapping        _irq;
			addr_t             _utcb;
			char               _name[32];       /* thread name that will be
			                                      registered at the kernel
			                                      debugger */
			Platform_pd       *_platform_pd;    /* protection domain thread
			                                      is bound to */
			Pager_object      *_pager_obj;
			unsigned           _prio;

			Affinity::Location _location;

			void _create_thread(void);
			void _finalize_construction(const char *name);
			bool _in_syscall(Fiasco::l4_umword_t flags);

		public:

			enum { DEFAULT_PRIORITY = 128 };

			/**
			 * Constructor for non-core threads
			 */
			Platform_thread(size_t, const char *name, unsigned priority,
			                Affinity::Location, addr_t);

			/**
			 * Constructor for core main-thread
			 */
			Platform_thread(Core_cap_index* thread,
			                Core_cap_index* irq, const char *name);

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
			 * Enable/disable single stepping
			 */
			void single_step(bool);

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
			 * \param pd    platform pd, thread is bound to
			 */
			void bind(Platform_pd *pd);

			/**
			 * Unbind this thread
			 */
			void unbind();

			/**
			 * Override thread state with 's'
			 *
			 * \throw Cpu_session::State_access_failed
			 */
			void state(Thread_state s);

			/**
			 * Read thread state
			 *
			 * \throw Cpu_session::State_access_failed
			 */
			Foc_thread_state state();

			/**
			 * Set the executing CPU for this thread
			 */
			void affinity(Affinity::Location location);

			/**
			 * Get the executing CPU for this thread
			 */
			Affinity::Location affinity() const;

			/************************
			 ** Accessor functions **
			 ************************/

			/**
			 * Return/set pager
			 */
			Pager_object *pager() const { return _pager_obj; }
			void pager(Pager_object *pager);

			/**
			 * Return identification of thread when faulting
			 */
			unsigned long pager_object_badge() {
				return (unsigned long) _thread.local.data()->kcap(); }

			/**
			 * Set CPU quota of the thread to 'quota'
			 */
			void quota(size_t const quota) { /* not supported*/ }

			/**
			 * Return execution time consumed by the thread
			 */
			unsigned long long execution_time() const { return 0; }


			/*******************************
			 ** Fiasco-specific Accessors **
			 *******************************/

			Cap_mapping const & thread()      const { return _thread;      }
			Cap_mapping       & gate()              { return _gate;        }
			const char         *name()        const { return _name;        }
			bool                core_thread() const { return _core_thread; }
			addr_t              utcb()        const { return _utcb;        }
	};
}

#endif /* _CORE__INCLUDE__PLATFORM_THREAD_H_ */
