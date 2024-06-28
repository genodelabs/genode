/*
 * \brief   Fiasco.OC thread facility
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
#include <base/trace/types.h>

/* core includes */
#include <pager.h>
#include <platform_pd.h>
#include <cap_mapping.h>
#include <assertion.h>

namespace Core {

	class Platform_pd;
	class Platform_thread;
}


class Core::Platform_thread : Interface
{
	private:

		/*
		 * Noncopyable
		 */
		Platform_thread(Platform_thread const &);
		Platform_thread &operator = (Platform_thread const &);

		enum State { DEAD, RUNNING };

		using Name = String<32>;

		friend class Platform_pd;

		Name    const _name;           /* name at kernel debugger */
		State         _state;
		bool          _core_thread;
		Cap_mapping   _thread;
		Cap_mapping   _gate  { };
		Cap_mapping   _pager { };
		Cap_mapping   _irq;
		addr_t        _utcb;
		Platform_pd  *_platform_pd;    /* protection domain thread is bound to */
		Pager_object *_pager_obj;
		unsigned      _prio;
		bool          _bound_to_pd = false;

		Affinity::Location _location { };

		void _create_thread(void);
		void _finalize_construction();
		bool _in_syscall(Foc::l4_umword_t flags);

	public:

		enum { DEFAULT_PRIORITY = 128 };

		/**
		 * Constructor for non-core threads
		 */
		Platform_thread(Platform_pd &, size_t, const char *name, unsigned priority,
		                Affinity::Location, addr_t);

		/**
		 * Constructor for core main-thread
		 */
		Platform_thread(Core_cap_index& thread,
		                Core_cap_index& irq, const char *name);

		/**
		 * Constructor for core threads
		 */
		Platform_thread(const char *name);

		/**
		 * Destructor
		 */
		~Platform_thread();

		/**
		 * Return true if thread creation succeeded
		 */
		bool valid() const { return _bound_to_pd; }

		/**
		 * Start thread
		 *
		 * \param ip  instruction pointer to start at
		 * \param sp  stack pointer to use
		 */
		void start(void *ip, void *sp);

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
		 * This thread is about to be bound
		 *
		 * \param pd    platform pd, thread is bound to
		 */
		void bind(Platform_pd &pd);

		/**
		 * Unbind this thread
		 */
		void unbind();

		/**
		 * Override thread state with 's'
		 */
		void state(Thread_state s);

		/**
		 * Read thread state
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

		/**
		 * Turn thread into vCPU
		 */
		Foc::l4_cap_idx_t setup_vcpu(unsigned, Cap_mapping const &,
		                             Cap_mapping &, addr_t &);


		/************************
		 ** Accessor functions **
		 ************************/

		/**
		 * Return/set pager
		 */
		Pager_object &pager() const
		{
			if (_pager_obj)
				return *_pager_obj;

			ASSERT_NEVER_CALLED;
		}

		void pager(Pager_object &pager);

		/**
		 * Return identification of thread when faulting
		 */
		unsigned long pager_object_badge()
		{
			return (unsigned long) _thread.local.data()->kcap();
		}

		/**
		 * Set CPU quota of the thread to 'quota'
		 */
		void quota(size_t const) { /* not supported*/ }

		/**
		 * Return execution time consumed by the thread
		 */
		Trace::Execution_time execution_time() const;


		/**********************************
		 ** Fiasco.OC-specific Accessors **
		 **********************************/

		Cap_mapping const & thread()      const { return _thread;      }
		Cap_mapping       & gate()              { return _gate;        }
		Name                name()        const { return _name;        }
		bool                core_thread() const { return _core_thread; }
		addr_t              utcb()        const { return _utcb;        }
		unsigned            prio()        const { return _prio;        }
};

#endif /* _CORE__INCLUDE__PLATFORM_THREAD_H_ */
