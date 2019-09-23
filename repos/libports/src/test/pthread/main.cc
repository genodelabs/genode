/*
 * \brief  POSIX thread and semaphore test
 * \author Christian Prochaska
 * \date   2012-04-04
 */

/*
 * Copyright (C) 2012-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <pthread.h>
#include <semaphore.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>


enum { NUM_THREADS = 2 };


struct Thread_args {
	int thread_num;
	sem_t thread_finished_sem;
	pthread_t thread_id_self; /* thread ID returned by 'pthread_self()' */
};


struct Thread {
	Thread_args thread_args;
	pthread_t thread_id_create; /* thread ID returned by 'pthread_create()' */
};


void *thread_func(void *arg)
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

void test_self_destruct(void *(*start_routine)(void*), uintptr_t num_iterations)
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

void *thread_func_self_destruct2(void *arg)
{
	return arg;
}

void *thread_func_self_destruct(void *arg)
{
	test_self_destruct(thread_func_self_destruct2, 2);

	return arg;
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
};

void *thread_mutex_func(void *arg)
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

	return 0;
}

void test_mutex()
{
	pthread_t t;

	Test_mutex_data test_mutex_data;

	if (pthread_create(&t, 0, thread_mutex_func, &test_mutex_data) != 0) {
		printf("error: pthread_create() failed\n");
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

int main(int argc, char **argv)
{
	printf("--- pthread test ---\n");

	pthread_t pthread_main = pthread_self();

	printf("main thread: running, my thread ID is %p\n", pthread_main);
	if (!pthread_main)
		return -1;

	Thread thread[NUM_THREADS];

	for (int i = 0; i < NUM_THREADS; i++) {
		thread[i].thread_args.thread_num = i + 1;

		printf("main thread: creating semaphore for thread %d\n",
		       thread[i].thread_args.thread_num);

		if (sem_init(&thread[i].thread_args.thread_finished_sem, 0, 1) != 0) {
			printf("sem_init() failed\n");
			return -1;
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
			return -1;
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
			return -1;
		}

	printf("main thread: destroying the threads\n");

	for (int i = 0; i < NUM_THREADS; i++) {

		void *retval;

		pthread_cancel(thread[i].thread_id_create);

		pthread_join(thread[i].thread_id_create, &retval);

		if (retval != PTHREAD_CANCELED) {
			printf("error: return value is not PTHREAD_CANCELED\n");
			return -1;
		}
	}

	printf("main thread: destroying the semaphores\n");

	for (int i = 0; i < NUM_THREADS; i++)
		sem_destroy(&thread[i].thread_args.thread_finished_sem);

	printf("main thread: create pthreads which self de-struct\n");

	test_self_destruct(thread_func_self_destruct, 100);

	printf("main thread: testing mutexes\n");

	test_mutex();

	printf("--- returning from main ---\n");
	return 0;
}
