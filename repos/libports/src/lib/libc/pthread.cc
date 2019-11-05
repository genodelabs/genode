/*
 * \brief  POSIX thread implementation
 * \author Christian Prochaska
 * \date   2012-03-12
 *
 */

/*
 * Copyright (C) 2012-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <base/log.h>
#include <base/sleep.h>
#include <base/thread.h>
#include <util/list.h>
#include <libc/allocator.h>

/* libc includes */
#include <errno.h>
#include <pthread.h>
#include <stdlib.h> /* malloc, free */

/* libc-internal includes */
#include <internal/pthread.h>
#include <internal/timed_semaphore.h>
#include <internal/init.h>
#include <internal/suspend.h>
#include <internal/resume.h>

using namespace Libc;


static Genode::Env *_env_ptr;  /* solely needed to spawn the timeout thread for the
                                  timed semaphore */

static Thread *_main_thread_ptr;

static Suspend *_suspend_ptr;
static Resume  *_resume_ptr;


void Libc::init_pthread_support(Genode::Env &env, Suspend &suspend, Resume &resume)
{
	_env_ptr         = &env;
	_main_thread_ptr = Thread::myself();
	_suspend_ptr     = &suspend;
	_resume_ptr      = &resume;
}


static Libc::Timeout_entrypoint &_global_timeout_ep()
{
	class Missing_call_of_init_pthread_support { };
	if (!_env_ptr)
		throw Missing_call_of_init_pthread_support();

	static Timeout_entrypoint timeout_ep { *_env_ptr };
	return timeout_ep;
}


/*************
 ** Pthread **
 *************/

void Libc::Pthread::Thread_object::entry()
{
	/* obtain stack attributes of new thread */
	Thread::Stack_info info = Thread::mystack();
	_stack_addr = (void *)info.base;
	_stack_size = info.top - info.base;

	pthread_exit(_start_routine(_arg));
}


void Libc::Pthread::join(void **retval)
{
	struct Check : Suspend_functor
	{
		bool retry { false };

		Pthread &_thread;

		Check(Pthread &thread) : _thread(thread) { }

		bool suspend() override
		{
			retry = !_thread._exiting;
			return retry;
		}
	} check(*this);

	struct Missing_call_of_init_pthread_support : Exception { };
	if (!_suspend_ptr)
		throw Missing_call_of_init_pthread_support();

	do {
		_suspend_ptr->suspend(check);
	} while (check.retry);

	_join_lock.lock();

	if (retval)
		*retval = _retval;
}


void Libc::Pthread::cancel()
{
	_exiting = true;

	struct Missing_call_of_init_pthread_support : Exception { };
	if (!_resume_ptr)
		throw Missing_call_of_init_pthread_support();

	_resume_ptr->resume_all();

	_join_lock.unlock();
}


/*
 * Registry
 */

void Libc::Pthread_registry::insert(Pthread &thread)
{
	/* prevent multiple insertions at the same location */
	static Lock insert_lock;
	Lock::Guard insert_lock_guard(insert_lock);

	for (unsigned int i = 0; i < MAX_NUM_PTHREADS; i++) {
		if (_array[i] == 0) {
			_array[i] = &thread;
			return;
		}
	}

	error("pthread registry overflow, pthread_self() might fail");
}


void Libc::Pthread_registry::remove(Pthread &thread)
{
	for (unsigned int i = 0; i < MAX_NUM_PTHREADS; i++) {
		if (_array[i] == &thread) {
			_array[i] = 0;
			return;
		}
	}

	error("could not remove unknown pthread from registry");
}


bool Libc::Pthread_registry::contains(Pthread &thread)
{
	for (unsigned int i = 0; i < MAX_NUM_PTHREADS; i++)
		if (_array[i] == &thread)
			return true;

	return false;
}


Libc::Pthread_registry &pthread_registry()
{
	static Libc::Pthread_registry instance;
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
struct pthread_mutex
{
	pthread_t _owner { nullptr };
	Lock      _data_mutex;

	struct Missing_call_of_init_pthread_support : Exception { };

	void _suspend(Suspend_functor &func)
	{
		if (!_suspend_ptr)
			throw Missing_call_of_init_pthread_support();
		_suspend_ptr->suspend(func);
	}

	void _resume_all()
	{
		if (!_resume_ptr)
			throw Missing_call_of_init_pthread_support();
		_resume_ptr->resume_all();
	}

	pthread_mutex() { }

	virtual ~pthread_mutex() { }

	/*
	 * The behavior of the following function follows the "robust mutex"
	 * described IEEE Std 1003.1 POSIX.1-2017
	 * https://pubs.opengroup.org/onlinepubs/9699919799/functions/pthread_mutex_lock.html
	 */
	virtual int lock()    = 0;
	virtual int trylock() = 0;
	virtual int unlock()  = 0;
};


struct Libc::Pthread_mutex_normal : pthread_mutex
{
	int lock() override final
	{
		struct Try_lock : Suspend_functor
		{
			bool retry { false }; /* have to try after resume */

			Pthread_mutex_normal &_mutex;

			Try_lock(Pthread_mutex_normal &mutex) : _mutex(mutex) { }

			bool suspend() override
			{
				retry = _mutex.trylock() == EBUSY;
				return retry;
			}
		} try_lock(*this);

		do { _suspend(try_lock); } while (try_lock.retry);

		return 0;
	}

	int trylock() override final
	{
		Lock::Guard lock_guard(_data_mutex);

		if (!_owner) {
			_owner = pthread_self();
			return 0;
		}

		return EBUSY;
	}

	int unlock() override final
	{
		Lock::Guard lock_guard(_data_mutex);

		if (_owner != pthread_self())
			return EPERM;

		_owner = nullptr;
		_resume_all();

		return 0;
	}
};


struct Libc::Pthread_mutex_errorcheck : pthread_mutex
{
	int lock() override final
	{
		/*
		 * We can't use trylock() as it returns EBUSY also for the
		 * EDEADLK case.
		 */
		struct Try_lock : Suspend_functor
		{
			bool retry  { false }; /* have to try after resume */
			int  result { 0 };

			Pthread_mutex_errorcheck &_mutex;

			Try_lock(Pthread_mutex_errorcheck &mutex) : _mutex(mutex) { }

			bool suspend() override
			{
				Lock::Guard lock_guard(_mutex._data_mutex);

				if (!_mutex._owner) {
					_mutex._owner = pthread_self();
					retry         = false;
					result        = 0;
				} else if (_mutex._owner == pthread_self()) {
					retry  = false;
					result = EDEADLK;
				} else {
					retry = true;
				}

				return retry;
			}
		} try_lock(*this);

		do { _suspend(try_lock); } while (try_lock.retry);

		return try_lock.result;
	}

	int trylock() override final
	{
		Lock::Guard lock_guard(_data_mutex);

		if (!_owner) {
			_owner = pthread_self();
			return 0;
		}

		return EBUSY;
	}

	int unlock() override final
	{
		Lock::Guard lock_guard(_data_mutex);

		if (_owner != pthread_self())
			return EPERM;

		_owner = nullptr;
		_resume_all();

		return 0;
	}
};


struct Libc::Pthread_mutex_recursive : pthread_mutex
{
	unsigned _nesting_level { 0 };

	int lock() override final
	{
		struct Try_lock : Suspend_functor
		{
			bool retry { false }; /* have to try after resume */

			Pthread_mutex_recursive &_mutex;

			Try_lock(Pthread_mutex_recursive &mutex) : _mutex(mutex) { }

			bool suspend() override
			{
				retry = _mutex.trylock() == EBUSY;
				return retry;
			}
		} try_lock(*this);

		do { _suspend(try_lock); } while (try_lock.retry);

		return 0;
	}

	int trylock() override final
	{
		Lock::Guard lock_guard(_data_mutex);

		if (!_owner) {
			_owner         = pthread_self();
			_nesting_level = 1;
			return 0;
		} else if (_owner == pthread_self()) {
			++_nesting_level;
			return 0;
		}

		return EBUSY;
	}

	int unlock() override final
	{
		Lock::Guard lock_guard(_data_mutex);

		if (_owner != pthread_self())
			return EPERM;

		--_nesting_level;
		if (_nesting_level == 0) {
			_owner = nullptr;
			_resume_all();
		}

		return 0;
	}
};


extern "C" {

	/* Thread */

	int pthread_join(pthread_t thread, void **retval)
	{
		thread->join(retval);

		Libc::Allocator alloc { };
		destroy(alloc, thread);

		return 0;
	}


	int pthread_attr_init(pthread_attr_t *attr)
	{
		if (!attr)
			return EINVAL;

		Libc::Allocator alloc { };
		*attr = new (alloc) pthread_attr;

		return 0;
	}


	int pthread_attr_destroy(pthread_attr_t *attr)
	{
		if (!attr || !*attr)
			return EINVAL;

		Libc::Allocator alloc { };
		destroy(alloc, *attr);
		*attr = 0;

		return 0;
	}


	int pthread_cancel(pthread_t thread)
	{
		thread->cancel();
		return 0;
	}


	void pthread_exit(void *value_ptr)
	{
		pthread_self()->exit(value_ptr);
		sleep_forever();
	}


	/* special non-POSIX function (for example used in libresolv) */
	int _pthread_main_np(void)
	{
		return (Thread::myself() == _main_thread_ptr);
	}


	pthread_t pthread_self(void)
	{
		try {
			pthread_t pthread_myself =
				static_cast<pthread_t>(&Thread::Tls::Base::tls());

			if (pthread_registry().contains(*pthread_myself))
				return pthread_myself;
		}
		catch (Thread::Tls::Base::Undefined) { }

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
		 * allocating it in heap via new(). Otherwise, the static destruction
		 * of the pthread object would also destruct the 'Thread' of the main
		 * thread.
		 */
		Libc::Allocator alloc { };
		static pthread *main = new (alloc) pthread(*Thread::myself());
		return main;
	}


	pthread_t thr_self(void) { return pthread_self(); }

	__attribute__((alias("thr_self")))
	pthread_t __sys_thr_self(void);


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


	int pthread_attr_getstacksize(const pthread_attr_t *attr, size_t *stacksize)
	{
		void *stackaddr;
		return pthread_attr_getstack(attr, &stackaddr, stacksize);
	}


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


	/* Mutex */

	int pthread_mutexattr_init(pthread_mutexattr_t *attr)
	{
		if (!attr)
			return EINVAL;

		Libc::Allocator alloc { };
		*attr = new (alloc) pthread_mutex_attr { PTHREAD_MUTEX_NORMAL };

		return 0;
	}


	int pthread_mutexattr_destroy(pthread_mutexattr_t *attr)
	{
		if (!attr || !*attr)
			return EINVAL;

		Libc::Allocator alloc { };
		destroy(alloc, *attr);
		*attr = nullptr;

		return 0;
	}


	int pthread_mutexattr_settype(pthread_mutexattr_t *attr, int type)
	{
		if (!attr || !*attr)
			return EINVAL;

		(*attr)->type = (pthread_mutextype)type;

		return 0;
	}


	int pthread_mutex_init(pthread_mutex_t *mutex,
	                       pthread_mutexattr_t const *attr)
	{
		if (!mutex)
			return EINVAL;


		Libc::Allocator alloc { };

		pthread_mutextype const type = (!attr || !*attr)
		                             ? PTHREAD_MUTEX_NORMAL : (*attr)->type;
		switch (type) {
		case PTHREAD_MUTEX_NORMAL:     *mutex = new (alloc) Pthread_mutex_normal; break;
		case PTHREAD_MUTEX_ERRORCHECK: *mutex = new (alloc) Pthread_mutex_errorcheck; break;
		case PTHREAD_MUTEX_RECURSIVE:  *mutex = new (alloc) Pthread_mutex_recursive; break;

		default:
			*mutex = nullptr;
			return EINVAL;
		}

		return 0;
	}


	int pthread_mutex_destroy(pthread_mutex_t *mutex)
	{
		if ((!mutex) || (*mutex == PTHREAD_MUTEX_INITIALIZER))
			return EINVAL;

		Libc::Allocator alloc { };
		destroy(alloc, *mutex);
		*mutex = PTHREAD_MUTEX_INITIALIZER;

		return 0;
	}


	int pthread_mutex_lock(pthread_mutex_t *mutex)
	{
		if (!mutex)
			return EINVAL;

		if (*mutex == PTHREAD_MUTEX_INITIALIZER)
			pthread_mutex_init(mutex, nullptr);

		return (*mutex)->lock();
	}


	int pthread_mutex_trylock(pthread_mutex_t *mutex)
	{
		if (!mutex)
			return EINVAL;

		if (*mutex == PTHREAD_MUTEX_INITIALIZER)
			pthread_mutex_init(mutex, nullptr);

		return (*mutex)->trylock();
	}


	int pthread_mutex_unlock(pthread_mutex_t *mutex)
	{
		if (!mutex)
			return EINVAL;

		if (*mutex == PTHREAD_MUTEX_INITIALIZER)
			return EINVAL;

		return (*mutex)->unlock();
	}


	/* Condition variable */


	/*
	 * Implementation based on
	 * http://web.archive.org/web/20010914175514/http://www-classic.be.com/aboutbe/benewsletter/volume_III/Issue40.html#Workshop
	 */

	struct pthread_cond
	{
		int num_waiters;
		int num_signallers;
		Lock counter_lock;
		Timed_semaphore signal_sem { _global_timeout_ep() };
		Semaphore handshake_sem;

		pthread_cond() : num_waiters(0), num_signallers(0) { }
	};


	int pthread_condattr_init(pthread_condattr_t *attr)
	{
		if (!attr)
			return EINVAL;

		*attr = nullptr;

		return 0;
	}


	int pthread_condattr_destroy(pthread_condattr_t *attr)
	{
		/* assert that the attr was produced by the init no-op */
		if (!attr || *attr != nullptr)
			return EINVAL;

		return 0;
	}


	int pthread_condattr_setclock(pthread_condattr_t *attr,
	                              clockid_t clock_id)
	{
		/* assert that the attr was produced by the init no-op */
		if (!attr || *attr != nullptr)
			return EINVAL;

		warning(__func__, " not implemented yet");

		return 0;
	}


	static int cond_init(pthread_cond_t *__restrict cond,
	                     const pthread_condattr_t *__restrict attr)
	{
		static Lock cond_init_lock { };

		if (!cond)
			return EINVAL;

		try {
			Lock::Guard g(cond_init_lock);
			Libc::Allocator alloc { };
			*cond = new (alloc) pthread_cond;
			return 0;
		} catch (...) { return ENOMEM; }
	}


	int pthread_cond_init(pthread_cond_t *__restrict cond,
	                      const pthread_condattr_t *__restrict attr)
	{
		return cond_init(cond, attr);
	}


	int pthread_cond_destroy(pthread_cond_t *cond)
	{
		if (!cond || !*cond)
			return EINVAL;

		Libc::Allocator alloc { };
		destroy(alloc, *cond);
		*cond = 0;

		return 0;
	}


	static uint64_t timeout_ms(struct timespec currtime,
	                           struct timespec abstimeout)
	{
		enum { S_IN_MS = 1000, S_IN_NS = 1000 * 1000 * 1000 };

		if (currtime.tv_nsec >= S_IN_NS) {
			currtime.tv_sec  += currtime.tv_nsec / S_IN_NS;
			currtime.tv_nsec  = currtime.tv_nsec % S_IN_NS;
		}
		if (abstimeout.tv_nsec >= S_IN_NS) {
			abstimeout.tv_sec  += abstimeout.tv_nsec / S_IN_NS;
			abstimeout.tv_nsec  = abstimeout.tv_nsec % S_IN_NS;
		}

		/* check whether absolute timeout is in the past */
		if (currtime.tv_sec > abstimeout.tv_sec)
			return 0;

		uint64_t diff_ms = (abstimeout.tv_sec - currtime.tv_sec) * S_IN_MS;
		uint64_t diff_ns = 0;

		if (abstimeout.tv_nsec >= currtime.tv_nsec)
			diff_ns = abstimeout.tv_nsec - currtime.tv_nsec;
		else {
			/* check whether absolute timeout is in the past */
			if (diff_ms == 0)
				return 0;
			diff_ns  = S_IN_NS - currtime.tv_nsec + abstimeout.tv_nsec;
			diff_ms -= S_IN_MS;
		}

		diff_ms += diff_ns / 1000 / 1000;

		/* if there is any diff then let the timeout be at least 1 MS */
		if (diff_ms == 0 && diff_ns != 0)
			return 1;

		return diff_ms;
	}


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

		c->counter_lock.lock();
		c->num_waiters++;
		c->counter_lock.unlock();

		pthread_mutex_unlock(mutex);

		if (!abstime)
			c->signal_sem.down();
		else {
			struct timespec currtime;
			clock_gettime(CLOCK_REALTIME, &currtime);

			Alarm::Time timeout = timeout_ms(currtime, *abstime);

			try {
				c->signal_sem.down(timeout);
			} catch (Timeout_exception) {
				result = ETIMEDOUT;
			} catch (Nonblocking_exception) {
				errno  = ETIMEDOUT;
				result = ETIMEDOUT;
			}
		}

		c->counter_lock.lock();
		if (c->num_signallers > 0) {
			if (result == ETIMEDOUT) /* timeout occured */
				c->signal_sem.down();
			c->handshake_sem.up();
			--c->num_signallers;
		}
		c->num_waiters--;
		c->counter_lock.unlock();

		pthread_mutex_lock(mutex);

		return result;
	}


	int pthread_cond_wait(pthread_cond_t *__restrict cond,
	                      pthread_mutex_t *__restrict mutex)
	{
		return pthread_cond_timedwait(cond, mutex, 0);
	}


	int pthread_cond_signal(pthread_cond_t *cond)
	{
		if (!cond || !*cond)
			return EINVAL;

		pthread_cond *c = *cond;

		c->counter_lock.lock();
		if (c->num_waiters > c->num_signallers) {
		  ++c->num_signallers;
		  c->signal_sem.up();
		  c->counter_lock.unlock();
		  c->handshake_sem.down();
		} else
		  c->counter_lock.unlock();

	   return 0;
	}


	int pthread_cond_broadcast(pthread_cond_t *cond)
	{
		if (!cond || !*cond)
			return EINVAL;

		pthread_cond *c = *cond;

		c->counter_lock.lock();
		if (c->num_waiters > c->num_signallers) {
			int still_waiting = c->num_waiters - c->num_signallers;
			c->num_signallers = c->num_waiters;
			for (int i = 0; i < still_waiting; i++)
				c->signal_sem.up();
			c->counter_lock.unlock();
			for (int i = 0; i < still_waiting; i++)
				c->handshake_sem.down();
		} else
			c->counter_lock.unlock();

		return 0;
	}

	/* TLS */


	struct Key_element : List<Key_element>::Element
	{
		const void *thread_base;
		const void *value;

		Key_element(const void *thread_base, const void *value)
		: thread_base(thread_base),
		  value(value) { }
	};


	static Lock &key_list_lock()
	{
		static Lock inst { };
		return inst;
	}


	struct Keys
	{
		List<Key_element> key[PTHREAD_KEYS_MAX];
	};


	static Keys &keys()
	{
		static Keys inst { };
		return inst;
	}


	int pthread_key_create(pthread_key_t *key, void (*destructor)(void*))
	{
		if (!key)
			return EINVAL;

		Lock_guard<Lock> key_list_lock_guard(key_list_lock());

		for (int k = 0; k < PTHREAD_KEYS_MAX; k++) {
			/*
			 * Find an empty key slot and insert an element for the current
			 * thread to mark the key slot as used.
			 */
			if (!keys().key[k].first()) {
				Libc::Allocator alloc { };
				Key_element *key_element = new (alloc) Key_element(Thread::myself(), 0);
				keys().key[k].insert(key_element);
				*key = k;
				return 0;
			}
		}

		return EAGAIN;
	}


	int pthread_key_delete(pthread_key_t key)
	{
		if (key < 0 || key >= PTHREAD_KEYS_MAX || !keys().key[key].first())
			return EINVAL;

		Lock_guard<Lock> key_list_lock_guard(key_list_lock());

		while (Key_element * element = keys().key[key].first()) {
			keys().key[key].remove(element);
			Libc::Allocator alloc { };
			destroy(alloc, element);
		}

		return 0;
	}


	int pthread_setspecific(pthread_key_t key, const void *value)
	{
		if (key < 0 || key >= PTHREAD_KEYS_MAX)
			return EINVAL;

		void *myself = Thread::myself();

		Lock_guard<Lock> key_list_lock_guard(key_list_lock());

		for (Key_element *key_element = keys().key[key].first(); key_element;
		     key_element = key_element->next())
			if (key_element->thread_base == myself) {
				key_element->value = value;
				return 0;
			}

		/* key element does not exist yet - create a new one */
		Libc::Allocator alloc { };
		Key_element *key_element = new (alloc) Key_element(Thread::myself(), value);
		keys().key[key].insert(key_element);
		return 0;
	}


	void *pthread_getspecific(pthread_key_t key)
	{
		if (key < 0 || key >= PTHREAD_KEYS_MAX)
			return nullptr;

		void *myself = Thread::myself();

		Lock_guard<Lock> key_list_lock_guard(key_list_lock());

		for (Key_element *key_element = keys().key[key].first(); key_element;
		     key_element = key_element->next())
			if (key_element->thread_base == myself)
				return (void*)(key_element->value);

		return 0;
	}


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
				static Lock lock;
				Lock::Guard guard(lock);

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
}
