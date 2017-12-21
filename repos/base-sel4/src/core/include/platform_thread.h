/*
 * \brief   Thread facility
 * \author  Norman Feske
 * \date    2015-05-01
 */

/*
 * Copyright (C) 2015-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _CORE__INCLUDE__PLATFORM_THREAD_H_
#define _CORE__INCLUDE__PLATFORM_THREAD_H_

/* Genode includes */
#include <base/thread_state.h>
#include <util/string.h>

/* core includes */
#include <pager.h>
#include <ipc_pager.h>
#include <thread_sel4.h>
#include <install_mapping.h>

namespace Genode {

	class Platform_pd;
	class Platform_thread;
}


class Genode::Platform_thread : public List<Platform_thread>::Element
{
	private:

		/*
		 * Noncopyable
		 */
		Platform_thread(Platform_thread const &);
		Platform_thread &operator = (Platform_thread const &);

		Pager_object *_pager = nullptr;

		String<128> _name;

		/**
		 * Virtual address of the IPC buffer within the PDs address space
		 *
		 * The value is 0 for the PD's main thread. For all other threads,
		 * the value is somewhere within the stack area.
		 */
		addr_t const _utcb;

		Thread_info _info { };

		Cap_sel const _pager_obj_sel;

		/*
		 * Selectors within the PD's CSpace
		 *
		 * Allocated when the thread is started.
		 */
		Cap_sel _fault_handler_sel { 0 };
		Cap_sel _ep_sel            { 0 };
		Cap_sel _lock_sel          { 0 };

		friend class Platform_pd;

		Platform_pd *_pd = nullptr;

		enum { INITIAL_IPC_BUFFER_VIRT = 0x1000 };

		Affinity::Location _location;
		uint16_t           _priority;

	public:

		/**
		 * Constructor
		 */
		Platform_thread(size_t, const char *name = 0, unsigned priority = 0,
		                Affinity::Location = Affinity::Location(), addr_t utcb = 0);

		/**
		 * Destructor
		 */
		~Platform_thread();

		/**
		 * Start thread
		 *
		 * \param ip      instruction pointer to start at
		 * \param sp      stack pointer to use
		 * \param cpu_no  target cpu
		 *
		 * \retval  0  successful
		 * \retval -1  thread could not be started
		 */
		int start(void *ip, void *sp, unsigned int cpu_no = 0);

		/**
		 * Pause this thread
		 */
		void pause();

		/**
		 * Enable/disable single stepping
		 */
		void single_step(bool) { }

		/**
		 * Resume this thread
		 */
		void resume();

		/**
		 * Cancel currently blocking operation
		 */
		void cancel_blocking();

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
		Thread_state state();

		/**
		 * Return execution time consumed by the thread
		 */
		unsigned long long execution_time() const;


		/************************
		 ** Accessor functions **
		 ************************/

		/**
		 * Set pager capability
		 */
		Pager_object *pager(Pager_object *) const { return _pager; }
		void pager(Pager_object *pager) { _pager = pager; }
		Pager_object *pager() { return _pager; }

		/**
		 * Return identification of thread when faulting
		 */
		unsigned long pager_object_badge() const { return _pager_obj_sel.value(); }

		/**
		 * Set the executing CPU for this thread
		 */
		void affinity(Affinity::Location location);

		/**
		 * Get the executing CPU for this thread
		 */
		Affinity::Location affinity() const { return _location; }

		/**
		 * Set CPU quota of the thread
		 */
		void quota(size_t) { /* not supported */ }

		/**
		 * Get thread name
		 */
		const char *name() const { return _name.string(); }


		/*****************************
		 ** seL4-specific interface **
		 *****************************/

		Cap_sel tcb_sel() const { return _info.tcb_sel; }

		bool install_mapping(Mapping const &mapping);
};

#endif /* _CORE__INCLUDE__PLATFORM_THREAD_H_ */
