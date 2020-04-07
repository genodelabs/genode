/*
 * \brief  Thread interface
 * \author Norman Feske
 * \date   2006-04-28
 */

/*
 * Copyright (C) 2006-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__BASE__THREAD_H_
#define _INCLUDE__BASE__THREAD_H_

/* Genode includes */
#include <base/exception.h>
#include <base/blockade.h>
#include <base/trace/logger.h>
#include <cpu/consts.h>
#include <util/string.h>
#include <cpu_session/cpu_session.h>  /* for 'Thread_capability' type */

namespace Genode {
	struct Native_utcb;
	struct Native_thread;
	class Thread;
	class Stack;
	class Env;
	template <unsigned> class Thread_deprecated;
}


/**
 * Concurrent flow of control
 *
 * A 'Thread' object corresponds to a physical thread. The execution
 * starts at the 'entry()' method as soon as 'start()' is called.
 */
class Genode::Thread
{
	public:

		class Out_of_stack_space : public Exception { };
		class Stack_too_large    : public Exception { };
		class Stack_alloc_failed : public Exception { };

		typedef Affinity::Location  Location;
		typedef Cpu_session::Name   Name;
		typedef Cpu_session::Weight Weight;

		struct Stack_info { addr_t base; addr_t top; };

		struct Tls { struct Base; Base *ptr; };

	private:

		/**
		 * Allocate and locally attach a new stack
		 *
		 * \param stack_size   size of this threads stack
		 * \param main_thread  wether this is the main thread
		 */
		Stack *_alloc_stack(size_t stack_size, char const *name, bool main_thread);

		/**
		 * Detach and release stack of the thread
		 */
		void _free_stack(Stack *stack);

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

		/*
		 * Noncopyable
		 */
		Thread(Thread const &);
		Thread &operator = (Thread const &);

	protected:

		/**
		 * Capability for this thread (set by _start())
		 *
		 * Used if thread creation involves core's CPU service.
		 */
		Thread_capability _thread_cap { };

		/**
		 * Pointer to cpu session used for this thread
		 */
		Cpu_session *_cpu_session = nullptr;

		/**
		 * Session-local thread affinity
		 */
		Affinity::Location _affinity;

		/**
		 * Base pointer to Trace::Control area used by this thread
		 */
		Trace::Control *_trace_control = nullptr;

		/**
		 * Pointer to primary stack
		 */
		Stack *_stack = nullptr;

		/**
		 * Pointer to kernel-specific meta data
		 */
		Native_thread *_native_thread = nullptr;

		/**
		 * Blockade used for synchronizing the finalization of the thread
		 */
		Blockade _join { };

		/**
		 * Thread type
		 *
		 * Some threads need special treatment at construction. This enum
		 * is solely used to distinguish them at construction.
		 */
		enum Type { NORMAL, MAIN, REINITIALIZED_MAIN };

	private:

		Trace::Logger _trace_logger { };

		/**
		 * Return 'Trace::Logger' instance of calling thread
		 *
		 * This method is used by the tracing framework internally.
		 */
		static Trace::Logger *_logger();

		/**
		 * Base pointer to thread-local storage
		 *
		 * The opaque pointer allows higher-level thread libraries (i.e.,
		 * pthread) to implement TLS. It should never be used outside such
		 * libraries.
		 */
		Tls _tls { };

		friend class Tls::Base;

		/**
		 * Hook for platform-specific constructor supplements
		 *
		 * \param weight  weighting regarding the CPU session quota
		 * \param type    enables selection of special initialization
		 */
		void _init_platform_thread(size_t weight, Type type);

		void _init_cpu_session_and_trace_control();

	public:

		/**
		 * Constructor
		 *
		 * \noapi
		 *
		 * FIXME: With type = Forked_main_thread the stack allocation
		 *        gets skipped but we should at least set Stack::ds_cap in a
		 *        way that it references the dataspace of the already attached
		 *        stack.
		 *
		 * \deprecated  superseded by the 'Thread(Env &...' constructor
		 */
		Thread(size_t weight, const char *name, size_t stack_size,
		       Type type, Affinity::Location affinity = Affinity::Location());

		/**
		 * Constructor
		 *
		 * \noapi
		 *
		 * \param weight      weighting regarding the CPU session quota
		 * \param name        thread name (for debugging)
		 * \param stack_size  stack size
		 *
		 * \throw Stack_too_large
		 * \throw Stack_alloc_failed
		 * \throw Out_of_stack_space
		 *
		 * The stack for the new thread will be allocated from the RAM session
		 * of the component environment. A small portion of the stack size is
		 * internally used by the framework for storing thread-specific
		 * information such as the thread's name.
		 *
		 * \deprecated  superseded by the 'Thread(Env &...' constructor
		 */
		Thread(size_t weight, const char *name, size_t stack_size,
		       Affinity::Location affinity = Affinity::Location())
		: Thread(weight, name, stack_size, NORMAL, affinity) { }

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
		 * \throw Out_of_stack_space
		 *
		 * \deprecated  superseded by the 'Thread(Env &...' constructor
		 */
		Thread(size_t weight, const char *name, size_t stack_size,
		       Type type, Cpu_session *,
		       Affinity::Location affinity = Affinity::Location());

		/**
		 * Constructor
		 *
		 * \param env         component environment
		 * \param name        thread name, used for debugging
		 * \param stack_size  stack size
		 * \param location    CPU affinity relative to the CPU-session's
		 *                    affinity space
		 * \param weight      scheduling weight relative to the other threads
		 *                    sharing the same CPU session
		 * \param cpu_session CPU session used to create the thread. Normally
		 *                    'env.cpu()' should be specified.
		 *
		 * The 'env' argument is needed because the thread creation procedure
		 * needs to interact with the environment for attaching the thread's
		 * stack, the trace-control dataspace, and the thread's trace buffer
		 * and policy.
		 *
		 * \throw Stack_too_large
		 * \throw Stack_alloc_failed
		 * \throw Out_of_stack_space
		 */
		Thread(Env &env, Name const &name, size_t stack_size, Location location,
		       Weight weight, Cpu_session &cpu);

		/**
		 * Constructor
		 *
		 * This is a shortcut for the common case of creating a thread via
		 * the environment's CPU session, at the default affinity location, and
		 * with the default weight.
		 */
		Thread(Env &env, Name const &name, size_t stack_size);

		/**
		 * Destructor
		 */
		virtual ~Thread();

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
		 *
		 * \noapi
		 * \deprecated  use the 'Name name() const' method instead
		 */
		void name(char *dst, size_t dst_len);

		/**
		 * Request name of thread
		 */
		Name name() const;

		/**
		 * Add an additional stack to the thread
		 *
		 * \throw Stack_too_large
		 * \throw Stack_alloc_failed
		 * \throw Out_of_stack_space
		 *
		 * The stack for the new thread will be allocated from the RAM
		 * session of the component environment. A small portion of the
		 * stack size is internally used by the framework for storing
		 * thread-specific information such as the thread's name.
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
		Thread_capability cap() const { return _thread_cap; }

		/**
		 * Cancel currently blocking operation
		 */
		void cancel_blocking();

		/**
		 * Return kernel-specific thread meta data
		 */
		Native_thread &native_thread();

		/**
		 * Return top of primary stack
		 *
		 * \return  pointer just after first stack element
		 */
		void *stack_top() const;

		/**
		 * Return base of primary stack
		 *
		 * \return  pointer to last stack element
		 */
		void *stack_base() const;

		/**
		 * Return virtual size reserved for each stack within the stack area
		 */
		static size_t stack_virtual_size();

		/**
		 * Return the local base address of the stack area
		 */
		static addr_t stack_area_virtual_base();

		/**
		 * Return total size of the stack area
		 */
		static size_t stack_area_virtual_size();

		/**
		 * Return 'Thread' object corresponding to the calling thread
		 *
		 * \return  pointer to caller's 'Thread' object
		 */
		static Thread *myself();

		/**
		 * Return information about the current stack
		 */
		static Stack_info mystack();

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
		 * Log null-terminated string as trace event using log_output policy
		 *
		 * \return true if trace is really put to buffer
		 */
		static bool trace_captured(char const *cstring)
		{
			return _logger()->log_captured(cstring, strlen(cstring));
		}

		/**
		 * Log binary data as trace event
		 */
		static void trace(char const *data, size_t len)
		{
			_logger()->log(data, len);
		}

		/**
		 * Log trace event as defined in base/trace/events.h
		 */
		template <typename EVENT>
		static void trace(EVENT const *event) { _logger()->log(event); }

		/**
		 * Thread affinity
		 */
		Affinity::Location affinity() const { return _affinity; }
};


template <unsigned STACK_SIZE>
class Genode::Thread_deprecated : public Thread
{
	public:

		/**
		 * Constructor
		 *
		 * \param weight    weighting regarding the CPU session quota
		 * \param name      thread name (for debugging)
		 * \param type      enables selection of special construction
		 */
		explicit Thread_deprecated(size_t weight, const char *name)
		: Thread(weight, name, STACK_SIZE, Type::NORMAL) { }

		/**
		 * Constructor
		 *
		 * \param weight     weighting regarding the CPU session quota
		 * \param name       thread name (for debugging)
		 * \param type       enables selection of special construction
		 *
		 * \noapi
		 */
		explicit Thread_deprecated(size_t weight, const char *name, Type type)
		: Thread(weight, name, STACK_SIZE, type) { }

		/**
		 * Constructor
		 *
		 * \param weight       weighting regarding the CPU session quota
		 * \param name         thread name (for debugging)
		 * \param cpu_session  thread created via specific cpu session
		 *
		 * \noapi
		 */
		explicit Thread_deprecated(size_t weight, const char *name,
		                Cpu_session * cpu_session)
		: Thread(weight, name, STACK_SIZE, Type::NORMAL, cpu_session) { }

		/**
		 * Shortcut for 'Thread(DEFAULT_WEIGHT, name, type)'
		 *
		 * \noapi
		 */
		explicit Thread_deprecated(const char *name, Type type = NORMAL)
		: Thread(Weight::DEFAULT_WEIGHT, name, STACK_SIZE, type) { }

		/**
		 * Shortcut for 'Thread(DEFAULT_WEIGHT, name, cpu_session)'
		 *
		 * \noapi
		 */
		explicit Thread_deprecated(const char *name, Cpu_session * cpu_session)
		: Thread(Weight::DEFAULT_WEIGHT, name, STACK_SIZE,
		              Type::NORMAL, cpu_session)
		{ }
};

#endif /* _INCLUDE__BASE__THREAD_H_ */
