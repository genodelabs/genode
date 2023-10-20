/*
 * \brief  POSIX thread implementation
 * \author Christian Prochaska
 * \author Christian Helmuth
 * \date   2012-03-12
 */

/*
 * Copyright (C) 2012-2020 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <base/log.h>
#include <base/thread.h>
#include <util/list.h>
#include <libc/allocator.h>

/* Genode-internal includes */
#include <base/internal/unmanaged_singleton.h>

/* libc includes */
#include <errno.h>
#include <pthread.h>
#include <semaphore.h>
#include <stdio.h>  /* __isthreaded */
#include <stdlib.h> /* malloc, free */

/* libc-internal includes */
#include <internal/init.h>
#include <internal/kernel.h>
#include <internal/pthread.h>
#include <internal/monitor.h>
#include <internal/time.h>
#include <internal/timer.h>


using namespace Libc;


static Thread         *_main_thread_ptr;
static Monitor        *_monitor_ptr;
static Timer_accessor *_timer_accessor_ptr;


void Libc::init_pthread_support(Monitor &monitor, Timer_accessor &timer_accessor)
{
	_main_thread_ptr    = Thread::myself();
	_monitor_ptr        = &monitor;
	_timer_accessor_ptr = &timer_accessor;

	Pthread::init_tls_support();
}


static Libc::Monitor & monitor()
{
	struct Missing_call_of_init_pthread_support : Genode::Exception { };
	if (!_monitor_ptr)
		throw Missing_call_of_init_pthread_support();
	return *_monitor_ptr;
}

namespace { using Fn = Libc::Monitor::Function_result; }

/*************
 ** Pthread **
 *************/

size_t Pthread::_stack_virtual_base_mask;
size_t Pthread::_tls_pointer_offset;


void Libc::Pthread::Thread_object::entry()
{
	/* use threaded mode in FreeBSD libc code */
	__isthreaded = 1;

	/*
	 * Obtain stack attributes of new thread for
	 * 'pthread_attr_get_np()'
	 */
	Thread::Stack_info info = Thread::mystack();
	_stack_addr = (void *)info.base;
	_stack_size = info.top - info.base;

	_tls_pointer(&info, _pthread);

	pthread_exit(_start_routine(_arg));
}


void Libc::Pthread::_tls_pointer(void *stack_address, Pthread *pthread)
{
	addr_t stack_virtual_base = (addr_t)stack_address &
	                            _stack_virtual_base_mask;
	*(Pthread**)(stack_virtual_base + _tls_pointer_offset) = pthread;
}


Libc::Pthread::Pthread(Thread &existing_thread, void *stack_address)
:
	_thread(existing_thread)
{
	/* 
	 * Obtain stack attributes for 'pthread_attr_get_np()'
	 *
	 * Note: the values might be incorrect for VirtualBox EMT threads,
	 *       which have this constructor called from a different thread
	 *       than 'existing_thread'.
	 *
	 */
	Thread::Stack_info info = Thread::mystack();
	_stack_addr = (void *)info.base;
	_stack_size = info.top - info.base;

	_tls_pointer(stack_address, this);

	pthread_cleanup().cleanup();
}


void Libc::Pthread::init_tls_support()
{
	Thread::Stack_info info = Thread::mystack();
	_tls_pointer_offset = info.libc_tls_pointer_offset;
	_stack_virtual_base_mask = ~(Thread::stack_virtual_size() - 1);
}


Pthread *Libc::Pthread::myself()
{
	int stack_variable;
	addr_t stack_virtual_base = (addr_t)&stack_variable &
	                            _stack_virtual_base_mask;
	return *(Pthread**)(stack_virtual_base + _tls_pointer_offset);
}


void Libc::Pthread::join(void **retval)
{
	monitor().monitor([&] {
		Genode::Mutex::Guard guard(_mutex);

		if (!_exiting)
			return Fn::INCOMPLETE;

		if (retval)
			*retval = _retval;
		return Fn::COMPLETE;
	});
}


int Libc::Pthread::detach()
{
	_detach_blockade.wakeup();
	return 0;
}


void Libc::Pthread::cancel()
{
	Genode::Mutex::Guard guard(_mutex);

	_exiting = true;

	monitor().trigger_monitor_examination();
}


/*
 * Cleanup
 */

void Libc::Pthread_cleanup::cleanup(Pthread *new_cleanup_thread)
{
	static Mutex cleanup_mutex;
	Mutex::Guard guard(cleanup_mutex);

	if (_cleanup_thread) {
		Libc::Allocator alloc { };
		destroy(alloc, _cleanup_thread);
	}

	_cleanup_thread = new_cleanup_thread;
}


Libc::Pthread_cleanup &pthread_cleanup()
{
	static Libc::Pthread_cleanup instance;
	return instance;
}


/***********
 ** Mutex **
 ***********/

namespace Libc {
	struct Pthread_mutex_normal;
	struct Pthread_mutex_errorcheck;
	struct Pthread_mutex_recursive;
}

/*
 * This class is named 'struct pthread_mutex_attr' because the
 * 'pthread_mutexattr_t' type is defined as 'struct pthread_mutex_attr *'
 * in '_pthreadtypes.h'
 */
struct pthread_mutex_attr { pthread_mutextype type; };


/*
 * This class is named 'struct pthread_mutex' because the 'pthread_mutex_t'
 * type is defined as 'struct pthread_mutex *' in '_pthreadtypes.h'
 */
class pthread_mutex : Genode::Noncopyable
{
	private:

		struct Applicant : Genode::Noncopyable
	{
		pthread_t const thread;

		Applicant *next { nullptr };

		Libc::Blockade &blockade;

		Applicant(pthread_t thread, Libc::Blockade &blockade)
		: thread(thread), blockade(blockade)
		{ }
	};

		Applicant *_applicants { nullptr };

	protected:

		pthread_t _owner      { nullptr };
		Mutex     _data_mutex;

		/* _data_mutex must be hold when calling the following methods */

		void _append_applicant(Applicant *applicant)
		{
			Applicant **tail = &_applicants;

			for (; *tail; tail = &(*tail)->next) ;

			*tail = applicant;
		}

		void _remove_applicant(Applicant *applicant)
		{
			Applicant **a = &_applicants;

			for (; *a && *a != applicant; a = &(*a)->next) ;

			*a = applicant->next;
		}

		void _next_applicant_to_owner()
		{
			if (Applicant *next = _applicants) {
				_remove_applicant(next);
				_owner = next->thread;
				next->blockade.wakeup();
			} else {
				_owner = nullptr;
			}
		}

		bool _applicant_for_mutex(pthread_t thread, Libc::Blockade &blockade)
		{
			Applicant applicant { thread, blockade };

			_append_applicant(&applicant);

			_data_mutex.release();

			blockade.block();

			_data_mutex.acquire();

			if (blockade.woken_up()) {
				return true;
			} else {
				_remove_applicant(&applicant);
				return false;
			}
		}

		struct Missing_call_of_init_pthread_support : Exception { };

		Timer_accessor & _timer_accessor()
		{
			if (!_timer_accessor_ptr)
				throw Missing_call_of_init_pthread_support();
			return *_timer_accessor_ptr;
		}

		/**
		 * Enqueue current context as applicant for mutex
		 *
		 * Return true if mutex was acquired, false on timeout expiration.
		 */
		bool _apply_for_mutex(pthread_t thread, Libc::uint64_t timeout_ms)
		{
			if (Libc::Kernel::kernel().main_context()) {
				Main_blockade blockade { timeout_ms };
				return _applicant_for_mutex(thread, blockade);
			} else {
				Pthread_blockade blockade { _timer_accessor(), timeout_ms };
				return _applicant_for_mutex(thread, blockade);
			}
		}

	public:

		pthread_mutex() { }

		virtual ~pthread_mutex() { }

		/*
		 * The behavior of the following function follows the "robust mutex"
		 * described IEEE Std 1003.1 POSIX.1-2017
		 * https://pubs.opengroup.org/onlinepubs/9699919799/functions/pthread_mutex_lock.html
		 */
		virtual int lock()                      = 0;
		virtual int timedlock(timespec const &) = 0;
		virtual int trylock()                   = 0;
		virtual int unlock()                    = 0;
};


struct Libc::Pthread_mutex_normal : pthread_mutex
{
	/* unsynchronized try */
	int _try_lock(pthread_t thread)
	{
		if (!_owner) {
			_owner = thread;
			return 0;
		}

		return EBUSY;
	}

	int lock() override final
	{
		pthread_t const myself = pthread_self();

		Mutex::Guard guard(_data_mutex);

		/* fast path without lock contention */
		if (_try_lock(myself) == 0)
			return 0;

		_apply_for_mutex(myself, 0);

		return 0;
	}

	int timedlock(timespec const &abs_timeout) override final
	{
		pthread_t const myself = pthread_self();

		Mutex::Guard guard(_data_mutex);

		/* fast path without lock contention - does not check abstimeout according to spec */
		if (_try_lock(myself) == 0)
			return 0;

		timespec abs_now;
		clock_gettime(CLOCK_REALTIME, &abs_now);

		uint64_t const timeout_ms = calculate_relative_timeout_ms(abs_now, abs_timeout);
		if (!timeout_ms)
			return ETIMEDOUT;

		if (_apply_for_mutex(myself, timeout_ms))
			return 0;
		else
			return ETIMEDOUT;
	}

	int trylock() override final
	{
		pthread_t const myself = pthread_self();

		Mutex::Guard guard(_data_mutex);

		return _try_lock(myself);
	}

	int unlock() override final
	{
		Mutex::Guard guard(_data_mutex);

		if (_owner != pthread_self())
			return EPERM;

		_next_applicant_to_owner();

		return 0;
	}
};


struct Libc::Pthread_mutex_errorcheck : pthread_mutex
{
	/* unsynchronized try */
	int _try_lock(pthread_t thread)
	{
		if (!_owner) {
			_owner = thread;
			return 0;
		}

		return _owner == thread ? EDEADLK : EBUSY;
	}

	int lock() override final
	{
		pthread_t const myself = pthread_self();

		Mutex::Guard guard(_data_mutex);

		/* fast path without lock contention (or deadlock) */
		int const result = _try_lock(myself);
		if (!result || result == EDEADLK)
			return result;

		_apply_for_mutex(myself, 0);

		return 0;
	}

	int timedlock(timespec const &) override final
	{
		/* XXX not implemented yet */
		return ENOSYS;
	}

	int trylock() override final
	{
		pthread_t const myself = pthread_self();

		Mutex::Guard guard(_data_mutex);

		return _try_lock(myself);
	}

	int unlock() override final
	{
		Mutex::Guard guard(_data_mutex);

		if (_owner != pthread_self())
			return EPERM;

		_next_applicant_to_owner();

		return 0;
	}
};


struct Libc::Pthread_mutex_recursive : pthread_mutex
{
	unsigned _nesting_level { 0 };

	/* unsynchronized try */
	int _try_lock(pthread_t thread)
	{
		if (!_owner) {
			_owner = thread;
			return 0;
		} else if (_owner == thread) {
			++_nesting_level;
			return 0;
		}

		return EBUSY;
	}

	int lock() override final
	{
		pthread_t const myself = pthread_self();

		Mutex::Guard guard(_data_mutex);

		/* fast path without lock contention */
		if (_try_lock(myself) == 0)
			return 0;

		_apply_for_mutex(myself, 0);

		return 0;
	}

	int timedlock(timespec const &) override final
	{
		return ENOSYS;
	}

	int trylock() override final
	{
		pthread_t const myself = pthread_self();

		Mutex::Guard guard(_data_mutex);

		return _try_lock(myself);
	}

	int unlock() override final
	{
		Mutex::Guard guard(_data_mutex);

		if (_owner != pthread_self())
			return EPERM;

		if (_nesting_level == 0)
			_next_applicant_to_owner();
		else
			--_nesting_level;

		return 0;
	}
};


/*
 * The pthread_cond implementation uses the POSIX semaphore API
 * internally that does not have means to set the clock. For this
 * reason the private 'sem_set_clock' function is introduced,
 * see 'semaphore.cc' for the implementation.
*/
extern "C" int sem_set_clock(sem_t *sem, clockid_t clock_id);


/* TLS */

class Key_allocator : public Genode::Bit_allocator<PTHREAD_KEYS_MAX>
{
	private:

		Mutex _mutex;

	public:

		addr_t alloc_key()
		{
			Mutex::Guard guard(_mutex);
			return alloc();
		}

		void free_key(addr_t key)
		{
			Mutex::Guard guard(_mutex);
			free(key);
		}
};


static Key_allocator &key_allocator()
{
	static Key_allocator inst;
	return inst;
}

typedef void (*key_destructor_func)(void*);
static key_destructor_func key_destructors[PTHREAD_KEYS_MAX];

extern "C" {

	int pthread_key_create(pthread_key_t *key, void (*destructor)(void*))
	{
		if (!key)
			return EINVAL;

		try {
			*key = key_allocator().alloc_key();
			key_destructors[*key] = destructor;
			return 0;
		} catch (Key_allocator::Out_of_indices) {
			return EAGAIN;
		}
	}

	typeof(pthread_key_create) _pthread_key_create
		__attribute__((alias("pthread_key_create")));


	int pthread_key_delete(pthread_key_t key)
	{
		if (key < 0 || key >= PTHREAD_KEYS_MAX)
			return EINVAL;

		key_destructors[key] = nullptr;
		key_allocator().free_key(key);
		return 0;
	}

	typeof(pthread_key_delete) _pthread_key_delete
		__attribute__((alias("pthread_key_delete")));


	int pthread_setspecific(pthread_key_t key, const void *value)
	{
		if (key < 0 || key >= PTHREAD_KEYS_MAX)
			return EINVAL;

		pthread_t pthread_myself = pthread_self();
		pthread_myself->setspecific(key, value);
		return 0;
	}

	typeof(pthread_setspecific) _pthread_setspecific
		__attribute__((alias("pthread_setspecific")));


	void *pthread_getspecific(pthread_key_t key)
	{
		if (key < 0 || key >= PTHREAD_KEYS_MAX)
			return nullptr;

		pthread_t pthread_myself = pthread_self();
		return (void*)pthread_myself->getspecific(key);
	}

	typeof(pthread_getspecific) _pthread_getspecific
		__attribute__((alias("pthread_getspecific")));


	/* Thread */

	int pthread_join(pthread_t thread, void **retval)
	{
		thread->join(retval);

		Libc::Allocator alloc { };
		destroy(alloc, thread);

		return 0;
	}

	typeof(pthread_join) _pthread_join
		__attribute__((alias("pthread_join")));


	int pthread_attr_init(pthread_attr_t *attr)
	{
		if (!attr)
			return EINVAL;

		Libc::Allocator alloc { };
		*attr = new (alloc) pthread_attr;

		return 0;
	}

	typeof(pthread_attr_init) _pthread_attr_init
		__attribute__((alias("pthread_attr_init")));


	int pthread_attr_destroy(pthread_attr_t *attr)
	{
		if (!attr || !*attr)
			return EINVAL;

		Libc::Allocator alloc { };
		destroy(alloc, *attr);
		*attr = 0;

		return 0;
	}

	typeof(pthread_attr_destroy) _pthread_attr_destroy
		__attribute__((alias("pthread_attr_destroy")));


	int pthread_cancel(pthread_t thread)
	{
		thread->cancel();
		return 0;
	}


	/* implemented in libc contrib sources */
	void __cxa_thread_call_dtors();

	void pthread_exit(void *value_ptr)
	{
		/* call 'thread_local' destructors */

		__cxa_thread_call_dtors();

		/* call TLS key destructors */

		bool at_least_one_destructor_called;

		do {
			at_least_one_destructor_called = false;
			for (pthread_key_t key = 0; key < PTHREAD_KEYS_MAX; key++) {
				if (key_destructors[key]) {
					void *value = pthread_getspecific(key);
					if (value) {
						pthread_setspecific(key, nullptr);
						key_destructors[key](value);
						at_least_one_destructor_called = true;
					}
				}
			}
		} while (at_least_one_destructor_called);

		pthread_self()->exit(value_ptr);
	}

	typeof(pthread_exit) _pthread_exit
		__attribute__((alias("pthread_exit"), noreturn));


	/* special non-POSIX function (for example used in libresolv) */
	int _pthread_main_np(void)
	{
		return (Thread::myself() == _main_thread_ptr);
	}


	pthread_t pthread_self(void)
	{
		pthread_t pthread_myself = static_cast<pthread_t>(Pthread::myself());

		if (pthread_myself)
			return pthread_myself;

		/*
		 * We pass here if the main thread or an alien thread calls
		 * pthread_self(). So check for aliens (or other bugs) and opt-out
		 * early.
		 */
		if (!_pthread_main_np()) {
			error("pthread_self() called from alien thread named ",
			      "'", Thread::myself()->name().string(), "'");
			return nullptr;
		}

		/*
		 * We create a pthread object associated to the main thread's Thread
		 * object. We ensure the pthread object does never get deleted by
		 * allocating it as unmanaged_singleton. Otherwise, the static
		 * destruction of the pthread object would also destruct the 'Thread'
		 * of the main thread.
		 */
		return unmanaged_singleton<pthread>(*Thread::myself(), &pthread_myself);
	}

	typeof(pthread_self) _pthread_self
		__attribute__((alias("pthread_self")));


	pthread_t thr_self(void) { return pthread_self(); }

	__attribute__((alias("thr_self")))
	pthread_t __sys_thr_self(void);


	int pthread_attr_getdetachstate(const pthread_attr_t *attr, int *detachstate)
	{
		if (!attr || !*attr || !detachstate)
			return EINVAL;

		*detachstate = (*attr)->detach_state;

		return 0;
	}

	typeof(pthread_attr_getdetachstate) _pthread_attr_getdetachstate
		__attribute__((alias("pthread_attr_getdetachstate")));


	int pthread_attr_setdetachstate(pthread_attr_t *attr, int detachstate)
	{
		if (!attr || !*attr)
			return EINVAL;

		(*attr)->detach_state = detachstate;

		return 0;
	}

	typeof(pthread_attr_setdetachstate) _pthread_attr_setdetachstate
		__attribute__((alias("pthread_attr_setdetachstate")));


	int pthread_attr_setstacksize(pthread_attr_t *attr, size_t stacksize)
	{
		if (!attr || !*attr)
			return EINVAL;

		if (stacksize < 4096)
			return EINVAL;

		size_t max_stack = Thread::stack_virtual_size() - 4 * 4096;
		if (stacksize > max_stack) {
			warning(__func__, ": requested stack size is ", stacksize, " limiting to ", max_stack);
			stacksize = max_stack;
		}

		(*attr)->stack_size = align_addr(stacksize, 12);

		return 0;
	}

	typeof(pthread_attr_setstacksize) _pthread_attr_setstacksize
		__attribute__((alias("pthread_attr_setstacksize")));


	int pthread_attr_getstack(const pthread_attr_t *attr,
	                          void **stackaddr,
	                          ::size_t *stacksize)
	{
		if (!attr || !*attr || !stackaddr || !stacksize)
			return EINVAL;

		*stackaddr = (*attr)->stack_addr;
		*stacksize = (*attr)->stack_size;

		return 0;
	}


	int pthread_attr_getstackaddr(const pthread_attr_t *attr, void **stackaddr)
	{
		size_t stacksize;
		return pthread_attr_getstack(attr, stackaddr, &stacksize);
	}

	typeof(pthread_attr_getstackaddr) _pthread_attr_getstackaddr
		__attribute__((alias("pthread_attr_getstackaddr")));


	int pthread_attr_getstacksize(const pthread_attr_t *attr, size_t *stacksize)
	{
		void *stackaddr;
		return pthread_attr_getstack(attr, &stackaddr, stacksize);
	}

	typeof(pthread_attr_getstacksize) _pthread_attr_getstacksize
		__attribute__((alias("pthread_attr_getstacksize")));


	int pthread_attr_get_np(pthread_t pthread, pthread_attr_t *attr)
	{
		if (!attr)
			return EINVAL;

		(*attr)->stack_addr = pthread->stack_addr();
		(*attr)->stack_size = pthread->stack_size();

		return 0;
	}


	int pthread_equal(pthread_t t1, pthread_t t2)
	{
		return (t1 == t2);
	}

	typeof(pthread_equal) _pthread_equal
		__attribute__((alias("pthread_equal")));


	int pthread_detach(pthread_t thread)
	{
		return thread->detach();
	}

	typeof(pthread_detach) _pthread_detach
		__attribute__((alias("pthread_detach")));


	void __pthread_cleanup_push_imp(void (*routine)(void*), void *arg,
	                                struct _pthread_cleanup_info *)
	{
		pthread_self()->cleanup_push(routine, arg);

	}

	typeof(__pthread_cleanup_push_imp) ___pthread_cleanup_push_imp
		__attribute__((alias("__pthread_cleanup_push_imp")));


	void __pthread_cleanup_pop_imp(int execute)
	{
		pthread_self()->cleanup_pop(execute);
	}

	typeof(__pthread_cleanup_pop_imp) ___pthread_cleanup_pop_imp
		__attribute__((alias("__pthread_cleanup_pop_imp")));


	/* Mutex */

	int pthread_mutexattr_init(pthread_mutexattr_t *attr)
	{
		if (!attr)
			return EINVAL;

		Libc::Allocator alloc { };
		*attr = new (alloc) pthread_mutex_attr { PTHREAD_MUTEX_NORMAL };

		return 0;
	}

	typeof(pthread_mutexattr_init) _pthread_mutexattr_init
		__attribute__((alias("pthread_mutexattr_init")));


	int pthread_mutexattr_destroy(pthread_mutexattr_t *attr)
	{
		if (!attr || !*attr)
			return EINVAL;

		Libc::Allocator alloc { };
		destroy(alloc, *attr);
		*attr = nullptr;

		return 0;
	}

	typeof(pthread_mutexattr_destroy) _pthread_mutexattr_destroy
		__attribute__((alias("pthread_mutexattr_destroy")));


	int pthread_mutexattr_settype(pthread_mutexattr_t *attr, int type)
	{
		if (!attr || !*attr)
			return EINVAL;

		(*attr)->type = (pthread_mutextype)type;

		return 0;
	}

	typeof(pthread_mutexattr_settype) _pthread_mutexattr_settype
		__attribute__((alias("pthread_mutexattr_settype")));


	int pthread_mutex_init(pthread_mutex_t *mutex,
	                       pthread_mutexattr_t const *attr)
	{
		if (!mutex)
			return EINVAL;


		Libc::Allocator alloc { };

		pthread_mutextype const type = (!attr || !*attr)
		                             ? PTHREAD_MUTEX_NORMAL : (*attr)->type;
		switch (type) {
		case PTHREAD_MUTEX_NORMAL:      *mutex = new (alloc) Pthread_mutex_normal; break;
		case PTHREAD_MUTEX_ADAPTIVE_NP: *mutex = new (alloc) Pthread_mutex_normal; break;
		case PTHREAD_MUTEX_ERRORCHECK:  *mutex = new (alloc) Pthread_mutex_errorcheck; break;
		case PTHREAD_MUTEX_RECURSIVE:   *mutex = new (alloc) Pthread_mutex_recursive; break;

		default:
			*mutex = nullptr;
			return EINVAL;
		}

		return 0;
	}

	typeof(pthread_mutex_init) _pthread_mutex_init
		__attribute__((alias("pthread_mutex_init")));


	int pthread_mutex_destroy(pthread_mutex_t *mutex)
	{
		if ((!mutex) || (*mutex == PTHREAD_MUTEX_INITIALIZER))
			return EINVAL;

		Libc::Allocator alloc { };
		destroy(alloc, *mutex);
		*mutex = PTHREAD_MUTEX_INITIALIZER;

		return 0;
	}

	typeof(pthread_mutex_destroy) _pthread_mutex_destroy
		__attribute__((alias("pthread_mutex_destroy")));


	int pthread_mutex_lock(pthread_mutex_t *mutex)
	{
		if (!mutex)
			return EINVAL;

		if (*mutex == PTHREAD_MUTEX_INITIALIZER)
			pthread_mutex_init(mutex, nullptr);

		return (*mutex)->lock();
	}

	typeof(pthread_mutex_lock) _pthread_mutex_lock
		__attribute__((alias("pthread_mutex_lock")));


	int pthread_mutex_trylock(pthread_mutex_t *mutex)
	{
		if (!mutex)
			return EINVAL;

		if (*mutex == PTHREAD_MUTEX_INITIALIZER)
			pthread_mutex_init(mutex, nullptr);

		return (*mutex)->trylock();
	}

	typeof(pthread_mutex_trylock) _pthread_mutex_trylock
		__attribute__((alias("pthread_mutex_trylock")));


	int pthread_mutex_timedlock(pthread_mutex_t *mutex,
	                            struct timespec const *abstimeout)
	{
		if (!mutex)
			return EINVAL;

		if (*mutex == PTHREAD_MUTEX_INITIALIZER)
			pthread_mutex_init(mutex, nullptr);

		/* abstime must be non-null according to the spec */
		return (*mutex)->timedlock(*abstimeout);
	}


	int pthread_mutex_unlock(pthread_mutex_t *mutex)
	{
		if (!mutex)
			return EINVAL;

		if (*mutex == PTHREAD_MUTEX_INITIALIZER)
			return EINVAL;

		return (*mutex)->unlock();
	}

	typeof(pthread_mutex_unlock) _pthread_mutex_unlock
		__attribute__((alias("pthread_mutex_unlock")));


	/* Condition variable */


	/*
	 * Implementation based on
	 * http://web.archive.org/web/20010914175514/http://www-classic.be.com/aboutbe/benewsletter/volume_III/Issue40.html#Workshop
	 */

	struct pthread_cond
	{
		int             num_waiters;
		int             num_signallers;
		pthread_mutex_t counter_mutex;
		sem_t           signal_sem;
		sem_t           handshake_sem;

		pthread_cond(clockid_t clock_id) : num_waiters(0), num_signallers(0)
		{
			pthread_mutex_init(&counter_mutex, nullptr);
			sem_init(&signal_sem, 0, 0);
			sem_init(&handshake_sem, 0, 0);

			if (sem_set_clock(&signal_sem, clock_id)) {
				struct Invalid_timedwait_clock { };
				throw Invalid_timedwait_clock();
			}
		}

		~pthread_cond()
		{
			sem_destroy(&handshake_sem);
			sem_destroy(&signal_sem);
			pthread_mutex_destroy(&counter_mutex);
		}
	};


	struct pthread_cond_attr
	{
		clockid_t clock_id { CLOCK_REALTIME };
	};


	int pthread_condattr_init(pthread_condattr_t *attr)
	{
		static Mutex condattr_init_mutex { };

		if (!attr)
			return EINVAL;

		try {
			Mutex::Guard guard(condattr_init_mutex);
			Libc::Allocator alloc { };
			*attr = new (alloc) pthread_cond_attr;
			return 0;
		} catch (...) { return ENOMEM; }

		return 0;
	}


	int pthread_condattr_destroy(pthread_condattr_t *attr)
	{
		if (!attr)
			return EINVAL;

		Libc::Allocator alloc { };
		destroy(alloc, *attr);
		*attr = nullptr;

		return 0;
	}


	int pthread_condattr_setclock(pthread_condattr_t *attr,
	                              clockid_t clock_id)
	{
		if (!attr)
			return EINVAL;

		(*attr)->clock_id = clock_id;

		return 0;
	}


	static int cond_init(pthread_cond_t *__restrict cond,
	                     const pthread_condattr_t *__restrict attr)
	{
		static Mutex cond_init_mutex { };

		if (!cond)
			return EINVAL;

		try {
			Mutex::Guard guard(cond_init_mutex);
			Libc::Allocator alloc { };
			*cond = attr && *attr ? new (alloc) pthread_cond((*attr)->clock_id)
			                      : new (alloc) pthread_cond(CLOCK_REALTIME);
			return 0;
		} catch (...) { return ENOMEM; }
	}


	int pthread_cond_init(pthread_cond_t *__restrict cond,
	                      const pthread_condattr_t *__restrict attr)
	{
		return cond_init(cond, attr);
	}

	typeof(pthread_cond_init) _pthread_cond_init
		__attribute__((alias("pthread_cond_init")));


	int pthread_cond_destroy(pthread_cond_t *cond)
	{
		if (!cond)
			return EINVAL;

		if (*cond == PTHREAD_COND_INITIALIZER)
			return 0;

		Libc::Allocator alloc { };
		destroy(alloc, *cond);
		*cond = 0;

		return 0;
	}

	typeof(pthread_cond_destroy) _pthread_cond_destroy
		__attribute__((alias("pthread_cond_destroy")));


	int pthread_cond_timedwait(pthread_cond_t *__restrict cond,
	                           pthread_mutex_t *__restrict mutex,
	                           const struct timespec *__restrict abstime)
	{
		int result = 0;

		if (!cond)
			return EINVAL;

		if (*cond == PTHREAD_COND_INITIALIZER)
			cond_init(cond, NULL);

		pthread_cond *c = *cond;

		pthread_mutex_lock(&c->counter_mutex);
		c->num_waiters++;
		pthread_mutex_unlock(&c->counter_mutex);

		pthread_mutex_unlock(mutex);

		if (!abstime) {
			if (sem_wait(&c->signal_sem) == -1)
				result = errno;
		} else {
			if (sem_timedwait(&c->signal_sem, abstime) == -1)
				result = errno;
		}

		pthread_mutex_lock(&c->counter_mutex);
		if (c->num_signallers > 0) {
			if (result == ETIMEDOUT) {
				/*
				 * Another thread may have called pthread_cond_signal(),
				 * detected this waiter and posted 'signal_sem' concurrently.
				 *
				 * We can't consume this post by 'sem_wait(&c->signal_sem)'
				 * because a third thread may have consumed the post already
				 * above in sem_wait/timedwait(). So, we just do nothing here
				 * and accept the spurious wakeup on next
				 * pthread_cond_wait/timedwait().
				 */
			}
			sem_post(&c->handshake_sem);
			--c->num_signallers;
		}
		c->num_waiters--;
		pthread_mutex_unlock(&c->counter_mutex);

		pthread_mutex_lock(mutex);

		return result;
	}

	typeof(pthread_cond_timedwait) _pthread_cond_timedwait
		__attribute__((alias("pthread_cond_timedwait")));


	int pthread_cond_wait(pthread_cond_t *__restrict cond,
	                      pthread_mutex_t *__restrict mutex)
	{
		return pthread_cond_timedwait(cond, mutex, nullptr);
	}

	typeof(pthread_cond_wait) _pthread_cond_wait
		__attribute__((alias("pthread_cond_wait")));


	int pthread_cond_signal(pthread_cond_t *cond)
	{
		if (!cond)
			return EINVAL;

		if (*cond == PTHREAD_COND_INITIALIZER)
			cond_init(cond, NULL);

		pthread_cond *c = *cond;

		pthread_mutex_lock(&c->counter_mutex);
		if (c->num_waiters > c->num_signallers) {
			++c->num_signallers;
			sem_post(&c->signal_sem);
			pthread_mutex_unlock(&c->counter_mutex);
			sem_wait(&c->handshake_sem);
		} else
			pthread_mutex_unlock(&c->counter_mutex);

		return 0;
	}

	typeof(pthread_cond_signal) _pthread_cond_signal
		__attribute__((alias("pthread_cond_signal")));


	int pthread_cond_broadcast(pthread_cond_t *cond)
	{
		if (!cond)
			return EINVAL;

		if (*cond == PTHREAD_COND_INITIALIZER)
			cond_init(cond, NULL);

		pthread_cond *c = *cond;

		pthread_mutex_lock(&c->counter_mutex);
		if (c->num_waiters > c->num_signallers) {
			int still_waiting = c->num_waiters - c->num_signallers;
			c->num_signallers = c->num_waiters;
			for (int i = 0; i < still_waiting; i++)
				sem_post(&c->signal_sem);
			pthread_mutex_unlock(&c->counter_mutex);
			for (int i = 0; i < still_waiting; i++)
				sem_wait(&c->handshake_sem);
		} else
			pthread_mutex_unlock(&c->counter_mutex);

		return 0;
	}

	typeof(pthread_cond_broadcast) _pthread_cond_broadcast
		__attribute__((alias("pthread_cond_broadcast")));


	int pthread_once(pthread_once_t *once, void (*init_once)(void))
	{
		if (!once || ((once->state != PTHREAD_NEEDS_INIT) &&
		              (once->state != PTHREAD_DONE_INIT)))
			return EINVAL;

		if (!once->mutex) {
			pthread_mutex_t p;
			pthread_mutex_init(&p, nullptr);
			if (!p) return EINVAL;

			{
				static Mutex mutex;
				Mutex::Guard guard(mutex);

				if (!once->mutex) {
					once->mutex = p;
					p = nullptr;
				}
			}

			/*
			 * If another thread concurrently allocated a mutex and was faster,
			 * free our mutex since it is not used.
			 */
			if (p) pthread_mutex_destroy(&p);
		}

		once->mutex->lock();

		if (once->state == PTHREAD_DONE_INIT) {
			once->mutex->unlock();
			return 0;
		}

		init_once();

		once->state = PTHREAD_DONE_INIT;

		once->mutex->unlock();

		return 0;
	}

	typeof(pthread_once) _pthread_once
		__attribute__((alias("pthread_once")));
}
