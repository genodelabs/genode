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

#include <base/log.h>
#include <base/thread.h>
#include <os/timed_semaphore.h>
#include <util/list.h>

#include <errno.h>
#include <pthread.h>
#include <stdlib.h> /* malloc, free */
#include "thread.h"

using namespace Genode;

/*
 * Structure to handle self-destructing pthreads.
 */
struct thread_cleanup : List<thread_cleanup>::Element
{
	pthread_t thread;

	thread_cleanup(pthread_t t) : thread(t) { }

	~thread_cleanup() {
		if (thread)
			delete thread;
	}
};

static Lock pthread_cleanup_list_lock;
static List<thread_cleanup> pthread_cleanup_list;


void * operator new(__SIZE_TYPE__ size) { return malloc(size); }
void operator delete (void * p) { return free(p); }


/*
 * We initialize the main-thread pointer in a constructor depending on the
 * assumption that libpthread is loaded on application startup by ldso. During
 * this stage only the main thread is executed.
 */
static __attribute__((constructor)) Thread * main_thread()
{
	static Thread *thread = Thread::myself();

	return thread;
}


void Pthread_registry::insert(pthread_t thread)
{
	/* prevent multiple insertions at the same location */
	static Genode::Lock insert_lock;
	Genode::Lock::Guard insert_lock_guard(insert_lock);

	for (unsigned int i = 0; i < MAX_NUM_PTHREADS; i++) {
		if (_array[i] == 0) {
			_array[i] = thread;
			return;
		}
	}

	Genode::error("pthread registry overflow, pthread_self() might fail");
}


void Pthread_registry::remove(pthread_t thread)
{
	for (unsigned int i = 0; i < MAX_NUM_PTHREADS; i++) {
		if (_array[i] == thread) {
			_array[i] = 0;
			return;
		}
	}

	Genode::error("could not remove unknown pthread from registry");
}


bool Pthread_registry::contains(pthread_t thread)
{
	for (unsigned int i = 0; i < MAX_NUM_PTHREADS; i++)
		if (_array[i] == thread)
			return true;

	return false;
}


Pthread_registry &pthread_registry()
{
	static Pthread_registry instance;
	return instance;
}


extern "C" {

	/* Thread */


	int pthread_attr_init(pthread_attr_t *attr)
	{
		if (!attr)
			return EINVAL;

		*attr = new pthread_attr;

		return 0;
	}


	int pthread_attr_destroy(pthread_attr_t *attr)
	{
		if (!attr || !*attr)
			return EINVAL;

		delete *attr;
		*attr = 0;

		return 0;
	}


	void pthread_cleanup()
	{
		{
			Lock_guard<Lock> lock_guard(pthread_cleanup_list_lock);

			while (thread_cleanup * t = pthread_cleanup_list.first()) {
				pthread_cleanup_list.remove(t);
				delete t;
			}
		}
	}

	int pthread_cancel(pthread_t thread)
	{
		/* cleanup threads which tried to self-destruct */
		pthread_cleanup();

		if (pthread_equal(pthread_self(), thread)) {
			Lock_guard<Lock> lock_guard(pthread_cleanup_list_lock);
			pthread_cleanup_list.insert(new thread_cleanup(thread));
		} else
			delete thread;

		return 0;
	}

	void pthread_exit(void *value_ptr)
	{
		pthread_cancel(pthread_self());

		Lock lock;
		while (true) lock.lock();
	}


	/* special non-POSIX function (for example used in libresolv) */
	int _pthread_main_np(void)
	{
		return (Thread::myself() == main_thread());
	}


	pthread_t pthread_self(void)
	{
		Thread *myself = Thread::myself();

		pthread_t pthread_myself = static_cast<pthread_t>(myself);

		if (pthread_registry().contains(pthread_myself))
			return pthread_myself;

		/*
		 * We pass here if the main thread or an alien thread calls
		 * pthread_self(). So check for aliens (or other bugs) and opt-out
		 * early.
		 */

		if (!_pthread_main_np()) {
			error("pthread_self() called from alien thread named ",
			      "'", myself->name().string(), "'");

			return nullptr;
		}

		/*
		 * We create a pthread object containing a copy of main thread's
		 * Thread object. Therefore, we ensure the pthread object does not
		 * get deleted by allocating it in heap via new(). Otherwise, the
		 * static destruction of the pthread object would also destruct the
		 * 'Thread' of the main thread.
		 */

		static struct pthread_attr main_thread_attr;
		static struct pthread *main = new pthread(*myself, &main_thread_attr);

		return main;
	}


	int pthread_attr_getstack(const pthread_attr_t *attr,
	                          void **stackaddr,
	                          ::size_t *stacksize)
	{
		/* FIXME */
		warning("pthread_attr_getstack() called, might not work correctly");

		if (!attr || !*attr || !stackaddr || !stacksize)
			return EINVAL;

		pthread_t pthread = (*attr)->pthread;

		*stackaddr = pthread->stack_top();
		*stacksize = (addr_t)pthread->stack_top() - (addr_t)pthread->stack_base();

		return 0;
	}


	int pthread_attr_get_np(pthread_t pthread, pthread_attr_t *attr)
	{
		if (!attr)
			return EINVAL;

		*attr = pthread->_attr;
		return 0;
	}


	int pthread_equal(pthread_t t1, pthread_t t2)
	{
		return (t1 == t2);
	}


	/* Mutex */


	struct pthread_mutex_attr
	{
		int type;

		pthread_mutex_attr() : type(PTHREAD_MUTEX_NORMAL) { }
	};


	struct pthread_mutex
	{
		pthread_mutex_attr mutexattr;

		Lock mutex_lock;

		pthread_t owner;
		int       lock_count;
		Lock      owner_and_counter_lock;

		pthread_mutex(const pthread_mutexattr_t *__restrict attr)
		: owner(0),
		  lock_count(0)
		{
			if (attr && *attr)
				mutexattr = **attr;
		}

		int lock()
		{
			if (mutexattr.type == PTHREAD_MUTEX_RECURSIVE) {

				Lock::Guard lock_guard(owner_and_counter_lock);

				if (lock_count == 0) {
					owner = pthread_self();
					lock_count++;
					mutex_lock.lock();
					return 0;
				}

				/* the mutex is already locked */
				if (pthread_self() == owner) {
					lock_count++;
					return 0;
				} else {
					mutex_lock.lock();
					return 0;
				}
			}

			if (mutexattr.type == PTHREAD_MUTEX_ERRORCHECK) {

				Lock::Guard lock_guard(owner_and_counter_lock);

				if (lock_count == 0) {
					owner = pthread_self();
					mutex_lock.lock();
					return 0;
				}

				/* the mutex is already locked */
				if (pthread_self() != owner) {
					mutex_lock.lock();
					return 0;
				} else
					return EDEADLK;
			}

			/* PTHREAD_MUTEX_NORMAL or PTHREAD_MUTEX_DEFAULT */
			mutex_lock.lock();
			return 0;
		}

		int trylock()
		{
			if (mutexattr.type == PTHREAD_MUTEX_RECURSIVE) {

				Lock::Guard lock_guard(owner_and_counter_lock);

				if (lock_count == 0) {
					owner = pthread_self();
					lock_count++;
					mutex_lock.lock();
					return 0;
				}

				/* the mutex is already locked */
				if (pthread_self() == owner) {
					lock_count++;
					return 0;
				} else {
					return EBUSY;
				}
			}

			if (mutexattr.type == PTHREAD_MUTEX_ERRORCHECK) {

				Lock::Guard lock_guard(owner_and_counter_lock);

				if (lock_count == 0) {
					owner = pthread_self();
					mutex_lock.lock();
					return 0;
				}

				/* the mutex is already locked */
				if (pthread_self() != owner) {
					return EBUSY;
				} else
					return EDEADLK;
			}

			/* PTHREAD_MUTEX_NORMAL or PTHREAD_MUTEX_DEFAULT */
			Lock::Guard lock_guard(owner_and_counter_lock);

			if (lock_count == 0) {
				owner = pthread_self();
				mutex_lock.lock();
				return 0;
			}

			return EBUSY;
		}

		int unlock()
		{

			if (mutexattr.type == PTHREAD_MUTEX_RECURSIVE) {

				Lock::Guard lock_guard(owner_and_counter_lock);

				if (pthread_self() != owner)
					return EPERM;

				lock_count--;

				if (lock_count == 0) {
					owner = 0;
					mutex_lock.unlock();
				}

				return 0;
			}

			if (mutexattr.type == PTHREAD_MUTEX_ERRORCHECK) {

				Lock::Guard lock_guard(owner_and_counter_lock);

				if (pthread_self() != owner)
					return EPERM;

				owner = 0;
				mutex_lock.unlock();
				return 0;
			}

			/* PTHREAD_MUTEX_NORMAL or PTHREAD_MUTEX_DEFAULT */
			mutex_lock.unlock();
			return 0;
		}
	};


	int pthread_mutexattr_init(pthread_mutexattr_t *attr)
	{
		if (!attr)
			return EINVAL;

		*attr = new pthread_mutex_attr;

		return 0;
	}


	int pthread_mutexattr_destroy(pthread_mutexattr_t *attr)
	{
		if (!attr || !*attr)
			return EINVAL;

		delete *attr;
		*attr = 0;

		return 0;
	}


	int pthread_mutexattr_settype(pthread_mutexattr_t *attr, int type)
	{
		if (!attr || !*attr)
			return EINVAL;

		(*attr)->type = type;

		return 0;
	}


	int pthread_mutex_init(pthread_mutex_t *__restrict mutex,
	                       const pthread_mutexattr_t *__restrict attr)
	{
		if (!mutex)
			return EINVAL;

		*mutex = new pthread_mutex(attr);

		return 0;
	}


	int pthread_mutex_destroy(pthread_mutex_t *mutex)
	{
		if ((!mutex) || (*mutex == PTHREAD_MUTEX_INITIALIZER))
			return EINVAL;

		delete *mutex;
		*mutex = PTHREAD_MUTEX_INITIALIZER;

		return 0;
	}


	int pthread_mutex_lock(pthread_mutex_t *mutex)
	{
		if (!mutex)
			return EINVAL;

		if (*mutex == PTHREAD_MUTEX_INITIALIZER)
			pthread_mutex_init(mutex, 0);

		(*mutex)->lock();

		return 0;
	}


	int pthread_mutex_trylock(pthread_mutex_t *mutex)
	{
		if (!mutex)
			return EINVAL;

		if (*mutex == PTHREAD_MUTEX_INITIALIZER)
			pthread_mutex_init(mutex, 0);

		return (*mutex)->trylock();
	}


	int pthread_mutex_unlock(pthread_mutex_t *mutex)
	{
		if (!mutex)
			return EINVAL;

		if (*mutex == PTHREAD_MUTEX_INITIALIZER)
			pthread_mutex_init(mutex, 0);

		(*mutex)->unlock();

		return 0;
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
		Timed_semaphore signal_sem;
		Semaphore handshake_sem;

		pthread_cond() : num_waiters(0), num_signallers(0) { }
	};


	int pthread_condattr_init(pthread_condattr_t *attr)
	{
		if (!attr)
			return EINVAL;

		*attr = 0;

		return 0;
	}


	int pthread_condattr_destroy(pthread_condattr_t *attr)
	{
		if (!attr || !*attr)
			return EINVAL;

		warning(__func__, " not implemented yet");

		return 0;
	}


	int pthread_condattr_setclock(pthread_condattr_t *attr,
	                              clockid_t clock_id)
	{
		if (!attr || !*attr)
			return EINVAL;

		warning(__func__, " not implemented yet");

		return 0;
	}


	int pthread_cond_init(pthread_cond_t *__restrict cond,
	                      const pthread_condattr_t *__restrict attr)
	{
		if (!cond)
			return EINVAL;

		*cond = new pthread_cond;

		return 0;
	}


	int pthread_cond_destroy(pthread_cond_t *cond)
	{
		if (!cond || !*cond)
			return EINVAL;

		delete *cond;
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

		if (!cond || !*cond)
			return EINVAL;

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
			} catch (Genode::Nonblocking_exception) {
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


	static Lock key_list_lock;
	List<Key_element> key_list[PTHREAD_KEYS_MAX];

	int pthread_key_create(pthread_key_t *key, void (*destructor)(void*))
	{
		if (!key)
			return EINVAL;

		Lock_guard<Lock> key_list_lock_guard(key_list_lock);

		for (int k = 0; k < PTHREAD_KEYS_MAX; k++) {
			/*
			 * Find an empty key slot and insert an element for the current
			 * thread to mark the key slot as used.
			 */
			if (!key_list[k].first()) {
				Key_element *key_element = new Key_element(Thread::myself(), 0);
				key_list[k].insert(key_element);
				*key = k;
				return 0;
			}
		}

		return EAGAIN;
	}


	int pthread_key_delete(pthread_key_t key)
	{
		if (key < 0 || key >= PTHREAD_KEYS_MAX || !key_list[key].first())
			return EINVAL;

		Lock_guard<Lock> key_list_lock_guard(key_list_lock);

		while (Key_element * element = key_list[key].first()) {
			key_list[key].remove(element);
			delete element;
		}

		return 0;
	}


	int pthread_setspecific(pthread_key_t key, const void *value)
	{
		if (key < 0 || key >= PTHREAD_KEYS_MAX)
			return EINVAL;

		void *myself = Thread::myself();

		Lock_guard<Lock> key_list_lock_guard(key_list_lock);

		for (Key_element *key_element = key_list[key].first(); key_element;
		     key_element = key_element->next())
			if (key_element->thread_base == myself) {
				key_element->value = value;
				return 0;
			}

		/* key element does not exist yet - create a new one */
		Key_element *key_element = new Key_element(Thread::myself(), value);
		key_list[key].insert(key_element);
		return 0;
	}


	void *pthread_getspecific(pthread_key_t key)
	{
		if (key < 0 || key >= PTHREAD_KEYS_MAX)
			return nullptr;

		void *myself = Thread::myself();

		Lock_guard<Lock> key_list_lock_guard(key_list_lock);

		for (Key_element *key_element = key_list[key].first(); key_element;
		     key_element = key_element->next())
			if (key_element->thread_base == myself)
				return (void*)(key_element->value);

		return 0;
	}


	int pthread_once(pthread_once_t *once, void (*init_once)(void))
	{
		if (!once || ((once->state != PTHREAD_NEEDS_INIT) &&
		              (once->state != PTHREAD_DONE_INIT)))
			return EINTR;

		if (!once->mutex) {
			pthread_mutex_t p = new pthread_mutex(0);
			/* be paranoid */
			if (!p)
				return EINTR;

			static Lock lock;

			lock.lock();
			if (!once->mutex) {
				once->mutex = p;
				p = nullptr;
			}
			lock.unlock();

			/*
			 * If another thread concurrently allocated a mutex and was faster,
			 * free our mutex since it is not used.
			 */
			if (p)
				delete p;
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
