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

		using Location = Affinity::Location;
		using Name     = Cpu_session::Name;
		using Weight   = Cpu_session::Weight;

		enum class Stack_error { STACK_AREA_EXHAUSTED, STACK_TOO_LARGE };

		struct Stack_info { addr_t base; addr_t top;
		                    addr_t libc_tls_pointer_offset; };

		Name const name;

	private:

		using Alloc_stack_result = Unique_attempt<Stack &, Stack_error>;

		/**
		 * Allocate and locally attach a new stack
		 *
		 * \param stack_size   size of this threads stack
		 * \param main_thread  wether this is the main thread
		 */
		Alloc_stack_result _alloc_stack(size_t stack_size, Name const &name,
		                                bool main_thread);

		/**
		 * Detach and release stack of the thread
		 */
		void _free_stack(Stack &stack);

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
		void _deinit_native_thread(Stack &);

		/*
		 * Noncopyable
		 */
		Thread(Thread const &);
		Thread &operator = (Thread const &);

	protected:

		/**
		 * Capability for this thread or creation error (set by _start())
		 *
		 * Used if thread creation involves core's CPU service.
		 */
		Cpu_session::Create_thread_result _thread_cap =
			Cpu_session::Create_thread_error::DENIED;

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
		 * Primary stack
		 */
		Alloc_stack_result _stack = Stack_error::STACK_AREA_EXHAUSTED;

		/**
		 * Pointer to kernel-specific meta data
		 */
		Native_thread *_native_thread_ptr = nullptr;

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
		enum Type { NORMAL, MAIN };

	private:

		Trace::Logger _trace_logger { };

		/**
		 * Return 'Trace::Logger' instance of calling thread
		 *
		 * This method is used by the tracing framework internally.
		 */
		static Trace::Logger *_logger();

		static void _with_logger(auto const &fn)
		{
			Trace::Logger * const ptr = _logger();
			if (ptr) fn(*ptr);
		}

		/**
		 * Hook for platform-specific constructor supplements
		 *
		 * \param weight  weighting regarding the CPU session quota
		 * \param type    enables selection of special initialization
		 */
		void _init_native_thread(Stack &, size_t weight, Type type);

		void _init_cpu_session_and_trace_control();

	public:

		/**
		 * Constructor
		 *
		 * \noapi
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

		enum class Start_result { OK, DENIED };

		/**
		 * Start execution of the thread
		 *
		 * This method is virtual to enable the customization of threads
		 * used as server activation.
		 */
		virtual Start_result start();

		using Alloc_secondary_stack_result = Attempt<void *, Stack_error>;

		/**
		 * Add an additional stack to the thread
		 *
		 * The stack for the new thread will be allocated from the RAM
		 * session of the component environment. A small portion of the
		 * stack size is internally used by the framework for storing
		 * thread-specific information such as the thread's name.
		 *
		 * \return  pointer to the new stack's top
		 */
		Alloc_secondary_stack_result alloc_secondary_stack(Name const &name,
		                                                   size_t stack_size);

		/**
		 * Remove a secondary stack from the thread
		 */
		void free_secondary_stack(void* stack_addr);

		/**
		 * Request capability of thread
		 */
		Thread_capability cap() const
		{
			return _thread_cap.convert<Thread_capability>(
				[&] (Thread_capability cap) { return cap; },
				[&] (auto) {
					error("attempt to obtain cap of incomplete thread");
					return Thread_capability();
				});
		}

		/**
		 * Call 'fn' with kernel-specific 'Native_thread &' as argument,
		 * or 'invalid_fn' if the thread has not been successfully constructed
		 */
		auto with_native_thread(auto const &fn,
		                        auto const &invalid_fn) const -> decltype(invalid_fn())
		{
			if (_native_thread_ptr) {
				Native_thread &native_thread = *_native_thread_ptr;
				return fn(native_thread);
			}
			return invalid_fn();
		}

		/**
		 * Conditionally call 'fn' with kernel-specific 'Native_thread &'
		 */
		void with_native_thread(auto const &fn) const
		{
			with_native_thread(fn, [&] { });
		}

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

		using Info_result = Attempt<Stack_info, Stack_error>;

		/**
		 * Return information about the thread's stack
		 */
		Info_result info() const;

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

		using Stack_size_result = Attempt<size_t, Stack_error>;

		/**
		 * Ensure that the stack has a given size at the minimum
		 *
		 * \param size  minimum stack size
		 */
		Stack_size_result stack_size(size_t const size);

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
		 * Log null-terminated string as trace event using log_output policy
		 *
		 * \return true if trace is really put to buffer
		 */
		static bool trace_captured(char const *cstring)
		{
			bool result = false;
			_with_logger([&] (Trace::Logger &l) {
				result = l.log_captured(cstring, strlen(cstring)); });
			return result;
		}

		/**
		 * Log binary data as trace event
		 */
		static void trace(char const *data, size_t len)
		{
			_with_logger([&] (Trace::Logger &l) { l.log(data, len); });
		}

		/**
		 * Log null-terminated string as trace event
		 */
		static void trace(char const *cstring) { trace(cstring, strlen(cstring)); }

		/**
		 * Log trace event as defined in base/trace/events.h
		 */
		static void trace(auto const *event)
		{
			_with_logger([&] (Trace::Logger &l) { l.log(event); });
		}

		/**
		 * Thread affinity
		 */
		Affinity::Location affinity() const { return _affinity; }
};

#endif /* _INCLUDE__BASE__THREAD_H_ */
