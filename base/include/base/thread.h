/*
 * \brief  Thread interface
 * \author Norman Feske
 * \date   2006-04-28
 *
 * For storing thread-specific data (called thread context) such as the stack
 * and thread-local data, there is a dedicated portion of the virtual address
 * space. This portion is called thread-context area. Within the thread-context
 * area, each thread has a fixed-sized slot, a thread context. The layout of
 * each thread context looks as follows
 *
 * ! lower address
 * !   ...
 * !   ============================ <- aligned at 'CONTEXT_VIRTUAL_SIZE'
 * !
 * !             empty
 * !
 * !   ----------------------------
 * !
 * !             stack
 * !             (top)              <- initial stack pointer
 * !   ---------------------------- <- address of 'Context' object
 * !    additional context members
 * !   ----------------------------
 * !              UTCB
 * !   ============================ <- aligned at 'CONTEXT_VIRTUAL_SIZE'
 * !   ...
 * ! higher address
 *
 * On some platforms, a user-level thread-control block (UTCB) area contains
 * data shared between the user-level thread and the kernel. It is typically
 * used for transferring IPC message payload or for system-call arguments.
 * The additional context members are a reference to the corresponding
 * 'Thread_base' object and the name of the thread.
 *
 * The thread context is a virtual memory area, initially not backed by real
 * memory. When a new thread is created, an empty thread context gets assigned
 * to the new thread and populated with memory pages for the stack and the
 * additional context members. Note that this memory is allocated from the RAM
 * session of the process environment and not accounted for when using the
 * 'sizeof()' operand on a 'Thread_base' object.
 */

/*
 * Copyright (C) 2006-2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__BASE__THREAD_H_
#define _INCLUDE__BASE__THREAD_H_

/* Genode includes */
#include <base/exception.h>
#include <base/lock.h>
#include <base/native_types.h>
#include <util/list.h>
#include <ram_session/ram_session.h>  /* for 'Ram_dataspace_capability' type */
#include <cpu_session/cpu_session.h>  /* for 'Thread_capability' type */


namespace Genode {

	class Rm_session;

	/**
	 * Concurrent control flow
	 *
	 * A 'Thread_base' object corresponds to a physical thread. The execution
	 * starts at the 'entry()' function as soon as 'start()' is called.
	 */
	class Thread_base
	{
		public:

			class Context_alloc_failed : public Exception { };
			class Stack_too_large      : public Exception { };
			class Stack_alloc_failed   : public Exception { };

		private:

			/**
			 * List-element helper to enable inserting threads in a list
			 */
			List_element<Thread_base> _list_element;

		public:

			/**
			 * Thread context located within the thread-context area
			 *
			 * The end of a thread context is placed at a boundary aligned at
			 * 'CONTEXT_VIRTUAL_SIZE'.
			 */
			struct Context
			{
				/**
				 * Top of the stack
				 */
				long stack[];

				/**
				 * Pointer to corresponding 'Thread_base' object
				 */
				Thread_base *thread_base;

				/**
				 * Virtual address of the start of the stack
				 *
				 * This address is pointing to the begin of the dataspace used
				 * for backing the thread context except for the UTCB (which is
				 * managed by the kernel).
				 */
				addr_t stack_base;

				/**
				 * Dataspace containing the backing store for the thread context
				 *
				 * We keep the dataspace capability to be able to release the
				 * backing store on thread destruction.
				 */
				Ram_dataspace_capability ds_cap;

				/**
				 * Maximum length of thread name, including null-termination
				 */
				enum { NAME_LEN = 64 };

				/**
				 * Thread name, used for debugging
				 */
				char name[NAME_LEN];

				/*
				 * <- end of regular memory area
				 *
				 * The following part of the thread context is backed by
				 * kernel-managed memory. No member variables are allowed
				 * beyond this point.
				 */

				/**
				 * Kernel-specific user-level thread control block
				 */
				Native_utcb utcb;
			};

		private:

			/**
			 * Manage the allocation of thread contexts
			 *
			 * There exists only one instance of this class per process.
			 */
			class Context_allocator
			{
				private:

					List<List_element<Thread_base> > _threads;
					Lock                             _threads_lock;

					/**
					 * Detect if a context already exists at the specified address
					 */
					bool _is_in_use(addr_t base);

				public:

					/**
					 * Allocate thread context for specified thread
					 *
					 * \param thread  thread for which to allocate the new context
					 * \return        virtual address of new thread context, or
					 *                0 if the allocation failed
					 */
					Context *alloc(Thread_base *thread);

					/**
					 * Release thread context
					 */
					void free(Thread_base *thread);

					/**
					 * Return 'Context' object for a given base address
					 */
					static Context *base_to_context(addr_t base);

					/**
					 * Return base address of context containing the specified address
					 */
					static addr_t addr_to_base(void *addr);
			};

			/**
			 * Return thread-context allocator
			 */
			static Context_allocator *_context_allocator();

			/**
			 * Allocate and locally attach a new thread context
			 */
			Context *_alloc_context(size_t stack_size);

			/**
			 * Detach and release thread context of the thread
			 */
			void _free_context();

			/**
			 * Platform-specific thread-startup code
			 *
			 * On some platforms, each new thread has to perform a startup
			 * protocol, e.g., waiting for a message from the kernel. This hook
			 * function allows for the implementation of such protocols.
			 */
			void _thread_bootstrap();

			/**
			 * Helper for thread startup
			 */
			static void _thread_start();

			/**
			 * Hook for platform-specific constructor supplements
			 */
			void _init_platform_thread();

			/**
			 * Hook for platform-specific destructor supplements
			 */
			void _deinit_platform_thread();

			/* hook only used for microblaze kernel */
			void _init_context(Context* c);

		protected:

			/**
			 * Capability for this thread (set by _start())
			 *
			 * Used if thread creation involves core's CPU service.
			 * Currently, this is not the case for NOVA.
			 */
			Genode::Thread_capability _thread_cap;

			/**
			 * Pointer to corresponding thread context
			 */
			Context *_context;

			/**
			 * Physical thread ID
			 */
			Native_thread _tid;

		public:

			/**
			 * Constructor
			 *
			 * \param name        thread name for debugging
			 * \param stack_size  stack size
			 *
			 * \throw Stack_too_large
			 * \throw Stack_alloc_failed
			 * \throw Context_alloc_failed
			 *
			 * The stack for the new thread will be allocated from the RAM
			 * session of the process environment. A small portion of the
			 * stack size is internally used by the framework for storing
			 * thread-context information such as the thread's name (see
			 * 'struct Context').
			 */
			Thread_base(const char *name, size_t stack_size);

			/**
			 * Destructor
			 */
			virtual ~Thread_base();

			/**
			 * Entry function of the thread
			 */
			virtual void entry() = 0;

			/**
			 * Start execution of the thread
			 *
			 * This function is virtual to enable the customization of threads
			 * used as server activation.
			 */
			virtual void start();

			/**
			 * Request name of thread
			 */
			void name(char *dst, size_t dst_len);

			/**
			 * Request capability of thread
			 */
			Genode::Thread_capability cap() const { return _thread_cap; }

			/**
			 * Cancel currently blocking operation
			 */
			void cancel_blocking();

			/**
			 * Only to be called from platform-specific code
			 */
			Native_thread & tid() { return _tid; }

			/**
			 * Return top of stack
			 *
			 * \return  pointer to first stack element
			 */
			void *stack_top() { return &_context->stack[-1]; }

			/**
			 * Return 'Thread_base' object corresponding to the calling thread
			 *
			 * \return  pointer to 'Thread_base' object, or
			 *          0 if the calling thread is the main thread
			 */
			static Thread_base *myself();

			/**
			 * Return user-level thread control block
			 *
			 * Note that it is safe to call this function on the result of the
			 * 'myself' function. It handles the special case of 'myself' being
			 * 0 when called by the main thread.
			 */
			Native_utcb *utcb();
	};


	template <unsigned STACK_SIZE>
	class Thread : public Thread_base
	{
		public:

			/**
			 * Constructor
			 *
			 * \param name  thread name (for debugging)
			 */
			explicit Thread(const char *name = "<noname>")
			: Thread_base(name, STACK_SIZE) { }
	};
}

#endif /* _INCLUDE__BASE__THREAD_H_ */
