/*
 * \brief  POSIX thread and semaphore test
 * \author Christian Prochaska
 * \author Christian Helmuth
 * \date   2012-04-04
 */

/*
 * Copyright (C) 2012-2019 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* libc include */
#include <pthread.h>
#include <semaphore.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

/* Genode includes */
#include <base/log.h>
#include <base/thread.h>


struct Thread_args {
	int thread_num;
	sem_t thread_finished_sem;
	pthread_t thread_id_self; /* thread ID returned by 'pthread_self()' */
};


struct Thread {
	Thread_args thread_args;
	pthread_t thread_id_create; /* thread ID returned by 'pthread_create()' */
};


static void *thread_func(void *arg)
{
	Thread_args *thread_args = (Thread_args*)arg;

	printf("thread %d: running, my thread ID is %p\n",
	       thread_args->thread_num, pthread_self());

	thread_args->thread_id_self = pthread_self();

	sem_post(&thread_args->thread_finished_sem);

	/* sleep forever */
	sem_t sleep_sem;
	sem_init(&sleep_sem, 0, 0);
	sem_wait(&sleep_sem);

	return 0;
}


/*
 * Test self-destructing threads with 'pthread_join()', both when created and
 * joined by the main thread and when created and joined by a pthread.
 */

static void self_destruct_helper(void *(*start_routine)(void*), uintptr_t num_iterations)
{
	for (uintptr_t i = 0; i < num_iterations; i++) {

		pthread_t  t;
		void      *retval;

		if (pthread_create(&t, 0, start_routine, (void*)i) != 0) {
			printf("error: pthread_create() failed\n");
			exit(-1);
		}

		pthread_join(t, &retval);

		if (retval != (void*)i) {
			printf("error: return value does not match\n");
			exit(-1);
		}
	}
}

static void *thread_func_self_destruct2(void *arg)
{
	return arg;
}

static void *thread_func_self_destruct(void *arg)
{
	/* also test nesting of pthreads */
	self_destruct_helper(thread_func_self_destruct2, 2);

	return arg;
}


static void test_self_destruct()
{
	printf("main thread: create self-destructing pthreads\n");

	self_destruct_helper(thread_func_self_destruct, 100);
}


static inline void compare_semaphore_values(int reported_value, int expected_value)
{
	if (reported_value != expected_value) {
		printf("error: sem_getvalue() did not return the expected value\n");
		exit(-1);
	}
}


struct Test_mutex_data
{
	sem_t           main_thread_ready_sem;
	sem_t           test_thread_ready_sem;
	pthread_mutex_t recursive_mutex;
	pthread_mutex_t errorcheck_mutex;

	Test_mutex_data()
	{
		sem_init(&main_thread_ready_sem, 0, 0);
		sem_init(&test_thread_ready_sem, 0, 0);

		pthread_mutexattr_t recursive_mutex_attr;
		pthread_mutexattr_init(&recursive_mutex_attr);
		pthread_mutexattr_settype(&recursive_mutex_attr, PTHREAD_MUTEX_RECURSIVE);
		pthread_mutex_init(&recursive_mutex, &recursive_mutex_attr);
		pthread_mutexattr_destroy(&recursive_mutex_attr);

		pthread_mutexattr_t errorcheck_mutex_attr;
		pthread_mutexattr_init(&errorcheck_mutex_attr);
		pthread_mutexattr_settype(&errorcheck_mutex_attr, PTHREAD_MUTEX_ERRORCHECK);
		pthread_mutex_init(&errorcheck_mutex, &errorcheck_mutex_attr);
		pthread_mutexattr_destroy(&errorcheck_mutex_attr);
	}

	~Test_mutex_data()
	{
		pthread_mutex_destroy(&errorcheck_mutex);
		pthread_mutex_destroy(&recursive_mutex);
	}
};

static void *thread_mutex_func(void *arg)
{
	Test_mutex_data *test_mutex_data = (Test_mutex_data*)arg;

	/**************************
	 ** test recursive mutex **
	 **************************/

	/* test unlocking unlocked mutex - should fail */

	if (pthread_mutex_unlock(&test_mutex_data->recursive_mutex) == 0) {
		printf("Error: could unlock unlocked recursive mutex\n");
		exit(-1);
	}

	/* test locking once - should succeed */

	if (pthread_mutex_lock(&test_mutex_data->recursive_mutex) != 0) {
		printf("Error: could not lock recursive mutex\n");
		exit(-1);
	}

	/* test locking twice - should succeed */

	if (pthread_mutex_lock(&test_mutex_data->recursive_mutex) != 0) {
		printf("Error: could not lock recursive mutex twice\n");
		exit(-1);
	}

	/* test unlocking once - should succeed */

	if (pthread_mutex_unlock(&test_mutex_data->recursive_mutex) != 0) {
		printf("Error: could not unlock recursive mutex\n");
		exit(-1);
	}

	/* test unlocking twice - should succeed */

	if (pthread_mutex_unlock(&test_mutex_data->recursive_mutex) != 0) {
		printf("Error: could not unlock recursive mutex twice\n");
		exit(-1);
	}

	/* test unlocking a third time - should fail */

	if (pthread_mutex_unlock(&test_mutex_data->recursive_mutex) == 0) {
		printf("Error: could unlock recursive mutex a third time\n");
		exit(-1);
	}

	/* wake up main thread */
	sem_post(&test_mutex_data->test_thread_ready_sem);

	/* wait for main thread - it should have the mutex locked */
	sem_wait(&test_mutex_data->main_thread_ready_sem);

	/* test unlocking mutex which is locked by main thread - should fail */

	if (pthread_mutex_unlock(&test_mutex_data->recursive_mutex) == 0) {
		printf("Error: could unlock recursive mutex which is owned by other thread\n");
		exit(-1);
	}

	/* wake up main thread */
	sem_post(&test_mutex_data->test_thread_ready_sem);

	/* wait for main thread */
	sem_wait(&test_mutex_data->main_thread_ready_sem);

	/***************************
	 ** test errorcheck mutex **
	 ***************************/

	/* test unlocking unlocked mutex - should fail */

	if (pthread_mutex_unlock(&test_mutex_data->errorcheck_mutex) == 0) {
		printf("Error: could unlock unlocked errorcheck mutex\n");
		exit(-1);
	}

	/* test locking once - should succeed */

	if (pthread_mutex_lock(&test_mutex_data->errorcheck_mutex) != 0) {
		printf("Error: could not lock errorcheck mutex\n");
		exit(-1);
	}

	/* test locking twice - should fail */

	if (pthread_mutex_lock(&test_mutex_data->errorcheck_mutex) == 0) {
		printf("Error: could lock errorcheck mutex twice\n");
		exit(-1);
	}

	/* test unlocking once - should succeed */

	if (pthread_mutex_unlock(&test_mutex_data->errorcheck_mutex) != 0) {
		printf("Error: could not unlock errorcheck mutex\n");
		exit(-1);
	}

	/* test unlocking twice - should fail */

	if (pthread_mutex_unlock(&test_mutex_data->errorcheck_mutex) == 0) {
		printf("Error: could unlock errorcheck mutex twice\n");
		exit(-1);
	}

	/* wake up main thread */
	sem_post(&test_mutex_data->test_thread_ready_sem);

	/* wait for main thread - it should have the mutex locked */
	sem_wait(&test_mutex_data->main_thread_ready_sem);

	/* test unlocking mutex which is locked by main thread */

	if (pthread_mutex_unlock(&test_mutex_data->errorcheck_mutex) == 0) {
		printf("Error: could unlock errorcheck mutex which is locked by other thread\n");
		exit(-1);
	}

	/* wake up main thread */
	sem_post(&test_mutex_data->test_thread_ready_sem);

	return nullptr;
}

static void test_mutex()
{
	printf("main thread: testing mutexes\n");

	pthread_t t;

	Test_mutex_data test_mutex_data;

	if (pthread_create(&t, 0, thread_mutex_func, &test_mutex_data) != 0) {
		printf("Error: pthread_create() failed\n");
		exit(-1);
	}

	/* wait for test thread - recursive mutex should be unlocked */
	sem_wait(&test_mutex_data.test_thread_ready_sem);

	/* lock the recursive mutex and let the test thread attempt to unlock it */

	if (pthread_mutex_lock(&test_mutex_data.recursive_mutex) != 0) {
		printf("Error: could not lock recursive mutex from main thread\n");
		exit(-1);
	}

	/* wake up test thread */
	sem_post(&test_mutex_data.main_thread_ready_sem);

	/* wait for test thread - recursive mutex should still be locked */
	sem_wait(&test_mutex_data.test_thread_ready_sem);

	/* unlock the recursive mutex - should succeed */

	if (pthread_mutex_unlock(&test_mutex_data.recursive_mutex) != 0) {
		printf("Error: could not unlock recursive mutex from main thread\n");
		exit(-1);
	}

	/* wake up test thread */
	sem_post(&test_mutex_data.main_thread_ready_sem);

	/* wait for test thread - errorcheck mutex should be unlocked */
	sem_wait(&test_mutex_data.test_thread_ready_sem);

	/* lock the errorcheck mutex and let the test thread attempt to unlock it */

	if (pthread_mutex_lock(&test_mutex_data.errorcheck_mutex) != 0) {
		printf("Error: could not lock errorcheck mutex from main thread\n");
		exit(-1);
	}

	/* wake up test thread */
	sem_post(&test_mutex_data.main_thread_ready_sem);

	/* wait for test thread - errorcheck mutex should still be locked */
	sem_wait(&test_mutex_data.test_thread_ready_sem);

	/* unlock the errorcheck mutex - should succeed */

	if (pthread_mutex_unlock(&test_mutex_data.errorcheck_mutex) != 0) {
		printf("Error: could not unlock errorcheck mutex from main thread\n");
		exit(-1);
	}

	pthread_join(t, NULL);
}


template <pthread_mutextype MUTEX_TYPE>
struct Mutex
{
	static const char *type_string()
	{
		switch (MUTEX_TYPE) {
		case PTHREAD_MUTEX_NORMAL:     return "PTHREAD_MUTEX_NORMAL";
		case PTHREAD_MUTEX_ERRORCHECK: return "PTHREAD_MUTEX_ERRORCHECK";
		case PTHREAD_MUTEX_RECURSIVE:  return "PTHREAD_MUTEX_RECURSIVE";

		default: break;
		}
		return "<unexpected mutex type>";
	};

	pthread_mutexattr_t _attr;
	pthread_mutex_t     _mutex;

	Mutex()
	{
		pthread_mutexattr_init(&_attr);
		pthread_mutexattr_settype(&_attr, MUTEX_TYPE);
		pthread_mutex_init(&_mutex, &_attr);
		pthread_mutexattr_destroy(&_attr);
	}

	~Mutex()
	{
		pthread_mutex_destroy(&_mutex);
	}

	pthread_mutex_t * mutex() { return &_mutex; }
};


template <pthread_mutextype MUTEX_TYPE>
struct Test_mutex_stress
{
	Mutex<MUTEX_TYPE> mutex;

	struct Thread
	{
		pthread_mutex_t *_mutex;
		sem_t            _startup_sem;
		pthread_t        _thread;

		static void * _entry_trampoline(void *arg)
		{
			Thread *t = (Thread *)arg;
			t->_entry();
			return nullptr;
		}

		void _lock()
		{
			if (int const err = pthread_mutex_lock(_mutex))
				Genode::error("lock() returned ", err);
		}

		void _unlock()
		{
			if (int const err = pthread_mutex_unlock(_mutex))
				Genode::error("unlock() returned ", err);
		}

		void _entry()
		{
			sem_wait(&_startup_sem);

			enum { ROUNDS = 800 };

			for (unsigned i = 0; i < ROUNDS; ++i) {
				_lock();
				if (MUTEX_TYPE == PTHREAD_MUTEX_RECURSIVE) {
					_lock();
					_lock();
				}

				/* stay in mutex for some time */
				for (unsigned volatile d = 0; d < 30000; ++d) ;

				if (MUTEX_TYPE == PTHREAD_MUTEX_RECURSIVE) {
					_unlock();
					_unlock();
				}
				_unlock();
			}
			Genode::log("thread ", this, ": ", (int)ROUNDS, " rounds done");
		}

		Thread(pthread_mutex_t *mutex) : _mutex(mutex)
		{
			sem_init(&_startup_sem, 0, 0);

			if (pthread_create(&_thread, 0, _entry_trampoline, this) != 0) {
				printf("Error: pthread_create() failed\n");
				exit(-1);
			}
		}

		void start() { sem_post(&_startup_sem); }
		void join()  { pthread_join(_thread, nullptr); }
	} threads[10] = {
		mutex.mutex(), mutex.mutex(), mutex.mutex(), mutex.mutex(), mutex.mutex(),
		mutex.mutex(), mutex.mutex(), mutex.mutex(), mutex.mutex(), mutex.mutex(),
	};

	Test_mutex_stress()
	{
		printf("main thread: start %s stress test\n", mutex.type_string());
		for (Thread &t : threads) t.start();
		for (Thread &t : threads) t.join();
		printf("main thread: finished %s stress test\n", mutex.type_string());
	}
};


extern "C" void wait_for_continue();

static void test_mutex_stress()
{
	printf("main thread: stressing mutexes\n");

	{ Test_mutex_stress<PTHREAD_MUTEX_NORMAL>     test_normal; }
	{ Test_mutex_stress<PTHREAD_MUTEX_ERRORCHECK> test_errorcheck; }
	{ Test_mutex_stress<PTHREAD_MUTEX_RECURSIVE>  test_recursive; }

	printf("main thread: mutex stress testing done\n");
};


/*
 * Test if the main thread resumes sleeping lock holders when it itself is
 * waiting for the lock.
 */

template <pthread_mutextype MUTEX_TYPE>
struct Test_lock_and_sleep
{
	sem_t             _startup;
	Mutex<MUTEX_TYPE> _mutex;

	enum { SLEEP_MS = 500 };

	static void *thread_fn(void *arg)
	{
		((Test_lock_and_sleep *)arg)->sleeper();
		return nullptr;
	}

	void sleeper()
	{
		printf("sleeper: aquire mutex\n");
		pthread_mutex_lock(_mutex.mutex());

		printf("sleeper: about to wake up main thread\n");
		sem_post(&_startup);

		printf("sleeper: sleep %u ms\n", SLEEP_MS);
		usleep(SLEEP_MS*1000);

		printf("sleeper: woke up, now release mutex\n");
		pthread_mutex_unlock(_mutex.mutex());
	}

	Test_lock_and_sleep()
	{
		sem_init(&_startup, 0, 0);

		printf("main thread: start %s test\n", _mutex.type_string());

		pthread_t id;
		if (pthread_create(&id, 0, thread_fn, this) != 0) {
			printf("error: pthread_create() failed\n");
			exit(-1);
		}

		sem_wait(&_startup);

		printf("main thread: sleeper woke me up, now aquire mutex (which blocks)\n");
		pthread_mutex_lock(_mutex.mutex());

		printf("main thread: aquired mutex, now release mutex and finish\n");
		pthread_mutex_unlock(_mutex.mutex());

		printf("main thread: finished %s test\n", _mutex.type_string());
	}
};


static void test_lock_and_sleep()
{
	printf("main thread: test resume in contended lock\n");

	{ Test_lock_and_sleep<PTHREAD_MUTEX_NORMAL>     test_normal; }
	{ Test_lock_and_sleep<PTHREAD_MUTEX_ERRORCHECK> test_errorcheck; }
	{ Test_lock_and_sleep<PTHREAD_MUTEX_RECURSIVE>  test_recursive; }

	printf("main thread: resume in contended lock testing done\n");
}


static void test_interplay()
{
	enum { NUM_THREADS = 2 };

	Thread thread[NUM_THREADS];

	for (int i = 0; i < NUM_THREADS; i++) {
		thread[i].thread_args.thread_num = i + 1;

		printf("main thread: creating semaphore for thread %d\n",
		       thread[i].thread_args.thread_num);

		if (sem_init(&thread[i].thread_args.thread_finished_sem, 0, 1) != 0) {
			printf("sem_init() failed\n");
			exit(-1);
		}

		/* check result of 'sem_getvalue()' before and after calling 'sem_wait()' */

		int sem_value = -1;

		sem_getvalue(&thread[i].thread_args.thread_finished_sem, &sem_value);
		compare_semaphore_values(sem_value, 1);

		sem_wait(&thread[i].thread_args.thread_finished_sem);

		sem_getvalue(&thread[i].thread_args.thread_finished_sem, &sem_value);
		compare_semaphore_values(sem_value, 0);

		thread[i].thread_args.thread_id_self = 0;

		printf("main thread: creating thread %d\n", thread[i].thread_args.thread_num);

		if (pthread_create(&thread[i].thread_id_create, 0, thread_func,
		                   &thread[i].thread_args) != 0) {
			printf("error: pthread_create() failed\n");
			exit(-1);
		}
		printf("main thread: thread %d has thread ID %p\n",
		       thread[i].thread_args.thread_num, thread[i].thread_id_create);
	}

	printf("main thread: waiting for the threads to finish\n");

	for (int i = 0; i < NUM_THREADS; i++)
		sem_wait(&thread[i].thread_args.thread_finished_sem);

	printf("main thread: comparing the thread IDs\n");

	for (int i = 0; i < NUM_THREADS; i++)
		if (thread[i].thread_args.thread_id_self != thread[i].thread_id_create) {
			printf("error: thread IDs don't match\n");
			exit(-1);
		}

	printf("main thread: destroying the threads\n");

	for (int i = 0; i < NUM_THREADS; i++) {

		void *retval;

		pthread_cancel(thread[i].thread_id_create);

		pthread_join(thread[i].thread_id_create, &retval);

		if (retval != PTHREAD_CANCELED) {
			printf("error: return value is not PTHREAD_CANCELED\n");
			exit(-1);
		}
	}

	printf("main thread: destroying the semaphores\n");

	for (int i = 0; i < NUM_THREADS; i++)
		sem_destroy(&thread[i].thread_args.thread_finished_sem);
}


int main(int argc, char **argv)
{
	printf("--- pthread test ---\n");

	pthread_t pthread_main = pthread_self();

	printf("main thread: running, my thread ID is %p\n", pthread_main);
	if (!pthread_main)
		exit(-1);

	test_interplay();
	test_self_destruct();
	test_mutex();
	test_mutex_stress();
	test_lock_and_sleep();

	printf("--- returning from main ---\n");
	return 0;
}
