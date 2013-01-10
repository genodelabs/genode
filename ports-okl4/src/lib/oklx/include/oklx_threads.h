/*
 * \brief  OKLinux library specific thread data
 * \author Stefan Kalkowski
 * \date   2009-05-28
 */

/*
 * Copyright (C) 2009-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _OKLINUX_SUPPORT__INCLUDE__OKLX_THREADS_H_
#define _OKLINUX_SUPPORT__INCLUDE__OKLX_THREADS_H_

/* Genode includes */
#include <base/sleep.h>
#include <base/thread.h>
#include <base/env.h>
#include <base/printf.h>
#include <util/list.h>
#include <rm_session/connection.h>
#include <cpu_session/connection.h>
#include <okl4_pd_session/connection.h>

namespace Okl4 {
extern "C" {

/* Iguana includes */
#include <iguana/eas.h>
#include <iguana/thread.h>

/* OKL4 includes */
#include <l4/ipc.h>
#include <l4/utcb.h>
}
}

namespace Genode {

	/**
	 * This class represents a OKLinux kernel thread
	 */
	class Oklx_kernel_thread : public List<Oklx_kernel_thread>::Element
	{
		private:

			/*
			 * We only need a small stack for startup code,
			 * afterwards OKLinux will rearrange the stack pointer
			 * to another memory area
			 */
			enum { STACK_SIZE=1024 };

			Thread_capability _cap;               /* Genodes thread capability */
			Okl4::L4_ThreadId_t _tid;

			char              _stack[STACK_SIZE]; /* stack for startup code */

		public:

			Oklx_kernel_thread(Thread_capability cap) : _cap(cap) {}

			Thread_capability cap() { return _cap; }

			void* stack_addr() { return (void*)&_stack[STACK_SIZE-1]; };

			static void entry();

			void set_tid(Okl4::L4_ThreadId_t tid) { _tid = tid; }

			Okl4::L4_ThreadId_t tid() { return _tid; }

	};


	/**
	 * An object of this class contains OKLinux kernel threads and
	 * an own cpu_session to create them.
	 */
	class Oklx_thread_list
	{
		private:

			List<Oklx_kernel_thread> _threads;
			Cpu_connection      _cpu;

		public:

			Okl4::L4_ThreadId_t add();
			Cpu_connection* cpu() { return &_cpu; }

			/**
			 * Get the global list of OKLinux kernel threads
			 */
			static Oklx_thread_list *thread_list();

			Oklx_kernel_thread * first() { return _threads.first(); }
    };


	/**
	 * This class represents an OKLinux process, and its threads.
	 */
	class Oklx_process : public List<Oklx_process>::Element
	{
		private:

			/**
			 * A thread within an OKLinux process
			 */
			class Oklx_user_thread : public List<Oklx_user_thread>::Element
			{
				private:

					friend class Oklx_process;

					Okl4::L4_ThreadId_t _tid;
					Thread_capability   _cap;

				public:

					Oklx_user_thread()
					: _cap(env()->cpu_session()->create_thread("Oklx user thread")) {}

					~Oklx_user_thread() {
						env()->cpu_session()->kill_thread(_cap); }

					Okl4::L4_ThreadId_t tid() { return _tid; }

					Thread_capability cap() { return _cap; }
			};


			Pd_connection          _pd;      /* protection domain of the process */
			Cpu_connection         _cpu;     /* cpu session to construct threads */
			List<Oklx_user_thread> _threads; /* list of all threads */
			Rm_connection          _rm;      /* rm session to manage addr. space */

		public:

			Oklx_process() { _pd.space_pager(pager_cap()); }

			~Oklx_process();

			Pd_connection* pd() { return &_pd; }

			Cpu_connection* cpu() { return &_cpu; }

			Rm_connection* rm() { return &_rm; }

			Okl4::L4_ThreadId_t add_thread();

			bool kill_thread(Okl4::L4_ThreadId_t tid);

			bool empty() { return !_threads.first(); }

			/**
			 * Get the global list of all OKLinux processes
			 */
			static List<Oklx_process> *processes();

			/**
			 * Get the capability of the pager thread,
			 * that pages OKLinux user processes (kernel main thread)
			 */
			static Thread_capability pager_cap();

			/**
			 * Set the capability of the pager thread,
			 * that pages OKLinux user processes (kernel main thread)
			 */
			static void set_pager();
	};
}

#endif //_OKLINUX_SUPPORT__INCLUDE__OKLX_THREADS_H_
