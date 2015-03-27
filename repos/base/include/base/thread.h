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
 * ; lower address
 * ;   ...
 * ;   ============================ <- aligned at the virtual context size
 * ;
 * ;             empty
 * ;
 * ;   ----------------------------
 * ;
 * ;             stack
 * ;             (top)              <- initial stack pointer
 * ;   ---------------------------- <- address of 'Context' object
 * ;    additional context members
 * ;   ----------------------------
 * ;              UTCB
 * ;   ============================ <- aligned at the virtual context size
 * ;   ...
 * ; higher address
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
 *
 * A thread may be associated with more than one stack. Additional secondary
 * stacks can be associated with a thread, and used for user level scheduling.
 */

/*
 * Copyright (C) 2006-2013 Genode Labs GmbH
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
#include <base/trace/logger.h>
#include <cpu/consts.h>
#include <util/string.h>
#include <util/bit_allocator.h>
#include <ram_session/ram_session.h>  /* for 'Ram_dataspace_capability' type */
#include <cpu_session/cpu_session.h>  /* for 'Thread_capability' type */
#include <cpu_session/capability.h>   /* for 'Cpu_session_capability' type */

namespace Genode {

	class Rm_session;
	class Thread_base;
	template <unsigned> class Thread;
}


/**
 * Concurrent control flow
 *
 * A 'Thread_base' object corresponds to a physical thread. The execution
 * starts at the 'entry()' method as soon as 'start()' is called.
 */
class Genode::Thread_base
{
	public:

		class Context_alloc_failed : public Exception { };
		class Stack_too_large      : public Exception { };
		class Stack_alloc_failed   : public Exception { };

		/**
		 * Thread context located within the thread-context area
		 *
		 * The end of a thread context is placed virtual size aligned.
		 */
		struct Context
		{
			private:

				/**
				 * Top of the stack is accessible via stack_top()
				 */
				long _stack[];

			public:

			/**
			 * Top of stack
			 *
			 * The alignment matches an initial stack frame, which is
			 * sufficient for the AMD64 ABI (stack top + adjustment is 16-byte
			 * aligned).
			 */
			addr_t stack_top() const
			{
				return ((addr_t)_stack & ~0xf) - Abi::stack_adjustment();
			}

			/**
			 * Ensure that the stack has a given size at the minimum
			 *
			 * \param size  minimum stack size
			 *
			 * \throw Stack_too_large
			 * \throw Stack_alloc_failed
			 */
			void stack_size(size_t const size);

			/**
			 * Virtual address of the start of the stack
			 *
			 * This address is pointing to the begin of the dataspace used
			 * for backing the thread context except for the UTCB (which is
			 * managed by the kernel).
			 */
			addr_t stack_base;

			/**
			 * Pointer to corresponding 'Thread_base' object
			 */
			Thread_base *thread_base;

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

				static constexpr size_t MAX_THREADS =
					Native_config::context_area_virtual_size() /
					Native_config::context_virtual_size();

				struct Context_bit_allocator : Bit_allocator<MAX_THREADS>
				{
					Context_bit_allocator()
					{
						/* the first index is used by main thread */
						_reserve(0, 1);
					}
				} _alloc;

				Lock _threads_lock;

			public:

				/**
				 * Allocate thread context for specified thread
				 *
				 * \param thread       thread for which to allocate the new context
				 * \param main_thread  wether to alloc for the main thread
				 *
				 * \return  virtual address of new thread context, or
				 *          0 if the allocation failed
				 */
				Context *alloc(Thread_base *thread, bool main_thread);

				/**
				 * Release thread context
				 */
				void free(Context *thread);

				/**
				 * Return 'Context' object for a given base address
				 */
				static Context *base_to_context(addr_t base);

				/**
				 * Return base address of context containing the specified address
				 */
				static addr_t addr_to_base(void *addr);

				/**
				 * Return index in context area for a given base address
				 */
				static size_t base_to_idx(addr_t base);

				/**
				 * Return base address of context given index in context area
				 */
				static addr_t idx_to_base(size_t idx);
		};

		/**
		 * Return thread-context allocator
		 */
		static Context_allocator *_context_allocator();

		/**
		 * Allocate and locally attach a new thread context
		 *
		 * \param stack_size   size of this threads stack
		 * \param main_thread  wether this is the main thread
		 */
		Context *_alloc_context(size_t stack_size, bool main_thread);

		/**
		 * Detach and release thread context of the thread
		 */
		void _free_context(Context *context);

		/**
		 * Platform-specific thread-startup code
		 *
		 * On some platforms, each new thread has to perform a startup
		 * protocol, e.g., waiting for a message from the kernel. This hook
		 * method allows for the implementation of such protocols.
		 */
		void _thread_bootstrap();

		/**
		 * Helper for thread startup
		 */
		static void _thread_start();

		/**
		 * Hook for platform-specific destructor supplements
		 */
		void _deinit_platform_thread();

	protected:

		/**
		 * Capability for this thread (set by _start())
		 *
		 * Used if thread creation involves core's CPU service.
		 */
		Genode::Thread_capability _thread_cap;

		/**
		 * Capability to pager paging this thread (created by _start())
		 */
		Genode::Pager_capability  _pager_cap;

		/**
		 * Pointer to cpu session used for this thread
		 */
		Genode::Cpu_session *_cpu_session;

		/**
		 * Pointer to primary thread context
		 */
		Context *_context;

		/**
		 * Physical thread ID
		 */
		Native_thread _tid;

		/**
		 * Lock used for synchronizing the finalization of the thread
		 */
		Genode::Lock _join_lock;

		/**
		 * Thread type
		 *
		 * Some threads need special treatment at construction. This enum
		 * is solely used to distinguish them at construction.
		 */
		enum Type { NORMAL, MAIN, REINITIALIZED_MAIN };

	private:

		Trace::Logger _trace_logger;

		/**
		 * Return 'Trace::Logger' instance of calling thread
		 *
		 * This method is used by the tracing framework internally.
		 */
		static Trace::Logger *_logger();

		/**
		 * Hook for platform-specific constructor supplements
		 *
		 * \param weight  weighting regarding the CPU session quota
		 * \param type    enables selection of special initialization
		 */
		void _init_platform_thread(size_t weight, Type type);

	public:

		/**
		 * Constructor
		 *
		 * \noapi
		 *
		 * FIXME: With type = Forked_main_thread the whole
		 *        Context::_alloc_context call gets skipped but we should
		 *        at least set Context::ds_cap in a way that it references
		 *        the dataspace of the already attached stack.
		 */
		Thread_base(size_t weight, const char *name, size_t stack_size,
		            Type type);

		/**
		 * Constructor
		 *
		 * \param weight      weighting regarding the CPU session quota
		 * \param name        thread name (for debugging)
		 * \param stack_size  stack size
		 *
		 * \throw Stack_too_large
		 * \throw Stack_alloc_failed
		 * \throw Context_alloc_failed
		 *
		 * The stack for the new thread will be allocated from the RAM session
		 * of the process environment. A small portion of the stack size is
		 * internally used by the framework for storing thread-context
		 * information such as the thread's name ('Context').
		 */
		Thread_base(size_t weight, const char *name, size_t stack_size)
		: Thread_base(weight, name, stack_size, NORMAL) { }

		/**
		 * Constructor
		 *
		 * Variant of the constructor that allows the use of a different
		 * CPU session.
		 *
		 * \noapi Using multiple CPU sessions within a single component is
		 *        an experimental feature.
		 *
		 * \param weight      weighting regarding the CPU session quota
		 * \param name        thread name (for debugging)
		 * \param stack_size  stack size
		 * \param type        enables selection of special construction
		 * \param cpu_session capability to cpu session used for construction
		 *
		 * \throw Stack_too_large
		 * \throw Stack_alloc_failed
		 * \throw Context_alloc_failed
		 */
		Thread_base(size_t weight, const char *name, size_t stack_size,
		            Type type, Cpu_session *);

		/**
		 * Destructor
		 */
		virtual ~Thread_base();

		/**
		 * Entry method of the thread
		 */
		virtual void entry() = 0;

		/**
		 * Start execution of the thread
		 *
		 * This method is virtual to enable the customization of threads
		 * used as server activation.
		 */
		virtual void start();

		/**
		 * Request name of thread
		 */
		void name(char *dst, size_t dst_len);

		/**
		 * Add an additional stack to the thread
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
		 *
		 * \return  pointer to the new stack's top
		 */
		void* alloc_secondary_stack(char const *name, size_t stack_size);

		/**
		 * Remove a secondary stack from the thread
		 */
		void free_secondary_stack(void* stack_addr);

		/**
		 * Request capability of thread
		 */
		Genode::Thread_capability cap() const { return _thread_cap; }

		/**
		 * Cancel currently blocking operation
		 */
		void cancel_blocking();

		/**
		 * Return thread ID
		 *
		 * \noapi  Only to be called from platform-specific code
		 */
		Native_thread & tid() { return _tid; }

		/**
		 * Return top of stack
		 *
		 * \return  pointer just after first stack element
		 */
		void *stack_top() const { return (void *)_context->stack_top(); }

		/**
		 * Return base of stack
		 *
		 * \return  pointer to last stack element
		 */
		void *stack_base() { return (void*)_context->stack_base; }

		/**
		 * Return 'Thread_base' object corresponding to the calling thread
		 *
		 * \return  pointer to caller's 'Thread_base' object
		 */
		static Thread_base *myself();

		/**
		 * Ensure that the stack has a given size at the minimum
		 *
		 * \param size  minimum stack size
		 *
		 * \throw Context::Stack_too_large
		 * \throw Context::Stack_alloc_failed
		 */
		void stack_size(size_t const size) { _context->stack_size(size); }

		/**
		 * Return user-level thread control block
		 *
		 * Note that it is safe to call this method on the result of the
		 * 'myself' class function. It handles the special case of 'myself'
		 * being 0 when called by the main thread during the component
		 * initialization phase.
		 */
		Native_utcb *utcb();

		/**
		 * Block until the thread leaves the 'entry' method
		 *
		 * Join must not be called more than once. Subsequent calls have
		 * undefined behaviour.
		 */
		void join();

		/**
		 * Log null-terminated string as trace event
		 */
		static void trace(char const *cstring)
		{
			_logger()->log(cstring, strlen(cstring));
		}

		/**
		 * Log binary data as trace event
		 */
		static void trace(char const *data, size_t len)
		{
			_logger()->log(data, len);
		}

		/**
		 * Log trace event as defined in base/trace.h
		 */
		template <typename EVENT>
		static void trace(EVENT const *event) { _logger()->log(event); }
};


template <unsigned STACK_SIZE>
class Genode::Thread : public Thread_base
{
	public:

		/**
		 * Constructor
		 *
		 * \param weight    weighting regarding the CPU session quota
		 * \param name      thread name (for debugging)
		 * \param type      enables selection of special construction
		 */
		explicit Thread(size_t weight, const char *name)
		: Thread_base(weight, name, STACK_SIZE, Type::NORMAL) { }

		/**
		 * Constructor
		 *
		 * \param weight     weighting regarding the CPU session quota
		 * \param name       thread name (for debugging)
		 * \param type       enables selection of special construction
		 *
		 * \noapi
		 */
		explicit Thread(size_t weight, const char *name, Type type)
		: Thread_base(weight, name, STACK_SIZE, type) { }

		/**
		 * Constructor
		 *
		 * \param weight       weighting regarding the CPU session quota
		 * \param name         thread name (for debugging)
		 * \param cpu_session  thread created via specific cpu session
		 *
		 * \noapi
		 */
		explicit Thread(size_t weight, const char *name,
		                Cpu_session * cpu_session)
		: Thread_base(weight, name, STACK_SIZE, Type::NORMAL, cpu_session) { }

		/**
		 * Shortcut for 'Thread(DEFAULT_WEIGHT, name, type)'
		 *
		 * \noapi
		 */
		explicit Thread(const char *name, Type type = NORMAL)
		: Thread_base(Cpu_session::DEFAULT_WEIGHT, name, STACK_SIZE, type) { }

		/**
		 * Shortcut for 'Thread(DEFAULT_WEIGHT, name, cpu_session)'
		 *
		 * \noapi
		 */
		explicit Thread(const char *name, Cpu_session * cpu_session)
		: Thread_base(Cpu_session::DEFAULT_WEIGHT, name, STACK_SIZE,
		              Type::NORMAL, cpu_session)
		{ }
};

#endif /* _INCLUDE__BASE__THREAD_H_ */
