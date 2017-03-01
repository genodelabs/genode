/*
 * \brief  Testing thread library
 * \author Alexander Boettcher
 * \author Christian Helmuth
 * \date   2013-12-13
 */

/*
 * Copyright (C) 2013-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <base/log.h>
#include <base/thread.h>
#include <base/component.h>
#include <base/heap.h>
#include <base/attached_rom_dataspace.h>
#include <util/reconstructible.h>
#include <cpu_session/connection.h>
#include <cpu_thread/client.h>

using namespace Genode;


/*********************************
 ** Stack-allocator concurrency **
 *********************************/

template <int CHILDREN>
struct Helper : Thread
{
	void *child[CHILDREN];

	enum { STACK_SIZE = 0x2000 };

	Env &_env;

	Helper(Env &env) : Thread(env, "helper", STACK_SIZE), _env(env) { }

	void *stack() const { return _stack; }

	void entry()
	{
		Constructible<Helper> helper[CHILDREN];

		for (unsigned i = 0; i < CHILDREN; ++i)
			helper[i].construct(_env);

		for (unsigned i = 0; i < CHILDREN; ++i)
			child[i] = helper[i]->stack();
	}
};


static void test_stack_alloc(Env &env)
{
	log("running '", __func__, "'");

	/*
	 * Create HELPER threads, which concurrently create CHILDREN threads each.
	 * This most likely triggers any race in the stack allocation.
	 */
	enum { HELPER = 10, CHILDREN = 9 };

	Constructible<Helper<CHILDREN> > helper[HELPER];

	for (unsigned i = 0; i < HELPER; ++i) helper[i].construct(env);
	for (unsigned i = 0; i < HELPER; ++i) helper[i]->start();
	for (unsigned i = 0; i < HELPER; ++i) helper[i]->join();

	if (0)
		for (unsigned i = 0; i < HELPER; ++i)
			for (unsigned j = 0; j < CHILDREN; ++j)
				log(helper[i]->child[j], " [", i, ".", j, "]");
}


/*********************
 ** Stack alignment **
 *********************/

/*
 * Aligned FPU instruction accesses are very useful to identify stack-alignment
 * issues. Fortunately, GCC generates pushes of FPU register content for
 * vararg functions if floating-point values are passed to the function.
 */

static void test_stack_alignment_varargs(char const *format, ...) __attribute__((noinline));
static void test_stack_alignment_varargs(char const *format, ...)
{
	va_list list;
	va_start(list, format);
	log(va_arg(list, double));
	log(va_arg(list, double));
	va_end(list);
}


static void log_stack_address(char const *who)
{
	long dummy;
	log(who, " stack @ ", &dummy);
}


struct Stack_helper : Thread
{
	enum { STACK_SIZE = 0x2000 };

	Stack_helper(Env &env) : Thread(env, "stack_helper", STACK_SIZE) { }

	void entry()
	{
		log_stack_address("helper");
		test_stack_alignment_varargs("%f\n%g\n", 3.142, 2.718);
	}
};


static void test_stack_alignment(Env &env)
{
	log("running '", __func__, "'");

	Stack_helper helper(env);

	helper.start();
	helper.join();

	log_stack_address("main");
	test_stack_alignment_varargs("%f\n%g\n", 3.142, 2.718);
}


/****************************
 ** Main-thread stack area **
 ****************************/

static void test_main_thread()
{
	log("running '", __func__, "'");

	/* check wether my thread object exists */
	Thread * myself = Thread::myself();
	if (!myself) { throw -1; }
	log("thread base          ", myself);

	/* check whether my stack is inside the first stack region */
	addr_t const stack_slot_base = Thread::stack_area_virtual_base();
	addr_t const stack_slot_size = Thread::stack_area_virtual_size();
	addr_t const stack_slot_top  = stack_slot_base + stack_slot_size;

	addr_t const stack_top  = (addr_t)myself->stack_top();
	addr_t const stack_base = (addr_t)myself->stack_base();

	if (stack_top  <= stack_slot_base) { throw -2; }
	if (stack_top  >  stack_slot_top)  { throw -3; }
	if (stack_base >= stack_slot_top)  { throw -4; }
	if (stack_base <  stack_slot_base) { throw -5; }

	log("thread stack top     ", myself->stack_top());
	log("thread stack bottom  ", myself->stack_base());

	/* check wether my stack pointer is inside my stack */
	unsigned dummy = 0;
	addr_t const sp = (addr_t)&dummy;
	if (sp >= stack_top)  { throw -6; }
	if (sp <  stack_base) { throw -7; }
	log("thread stack pointer ", (void *)sp);
}


/******************************************
 ** Using cpu-session for thread creation *
 ******************************************/

struct Cpu_helper : Thread
{
	enum { STACK_SIZE = 0x2000 };

	Env &_env;

	Cpu_helper(Env &env, const char * name, Cpu_session &cpu)
	:
		Thread(env, name, STACK_SIZE, Thread::Location(), Thread::Weight(), cpu),
		_env(env)
	{ }

	void entry()
	{
		log(Thread::name().string(), " : _cpu_session=", _cpu_session,
		    " env.cpu()=", &_env.cpu());
	}
};


static void test_cpu_session(Env &env)
{
	log("running '", __func__, "'");

	Cpu_helper thread0(env, "prio high  ", env.cpu());
	thread0.start();
	thread0.join();

	Cpu_connection con1(env, "prio middle", Cpu_session::PRIORITY_LIMIT / 4);
	Cpu_helper thread1(env, "prio middle", con1);
	thread1.start();
	thread1.join();

	Cpu_connection con2(env, "prio low", Cpu_session::PRIORITY_LIMIT / 2);
	Cpu_helper thread2(env, "prio low   ", con2);
	thread2.start();
	thread2.join();
}


struct Pause_helper : Thread
{
	volatile unsigned loop = 0;
	volatile bool beep = false;

	enum { STACK_SIZE = 0x2000 };

	Pause_helper(Env &env, const char * name, Cpu_session &cpu)
	: Thread(env, name, STACK_SIZE, Thread::Location(), Thread::Weight(), cpu) { }

	void entry()
	{
		while (1) {
			/*
			 * Don't log here, since this thread becomes "paused".
			 * If it is holding the lock of the log backend being paused, all
			 * other threads of this task trying to print log messages will
			 * block - looks like a deadlock.
			 */
			loop ++;
			if (beep) {
				log("beep");
				beep = false;
				loop ++;
				return;
			}
		}
	}
};


static void test_pause_resume(Env &env)
{
	log("running '", __func__, "'");

	Pause_helper thread(env, "pause", env.cpu());
	thread.start();

	while (thread.loop < 1) { }

	Thread_state state;
	Cpu_thread_client thread_client(thread.cap());

	log("--- pausing ---");
	thread_client.pause();
	unsigned loop_paused = thread.loop;
	log("--- paused ---");

	log("--- reading thread state ---");
	try {
		state = thread_client.state();
	} catch (Cpu_thread::State_access_failed) {
		throw -10;
	}
	if (loop_paused != thread.loop)
		throw -11;

	thread.beep = true;
	log("--- resuming thread ---");
	thread_client.resume();

	while (thread.loop == loop_paused) { }

	log("--- thread resumed ---");
	thread.join();
}


/*
 * Test to check that core as the used kernel behaves well if up to the
 * supported Genode maximum threads are created.
 */
static void test_create_as_many_threads(Env &env)
{
	log("running '", __func__, "'");

	addr_t const max = Thread::stack_area_virtual_size() /
	                   Thread::stack_virtual_size();

	Cpu_helper * threads[max];
	static char thread_name[8];

	Heap heap(env.ram(), env.rm());

	unsigned i = 0;
	try {
		for (; i < max; i++) {
			try {
				snprintf(thread_name, sizeof(thread_name), "%u", i + 1);
				threads[i] = new (heap) Cpu_helper(env, thread_name, env.cpu());
				threads[i]->start();
				threads[i]->join();
			} catch (Cpu_session::Thread_creation_failed) {
				throw "Thread_creation_failed";
			} catch (Thread::Out_of_stack_space) {
				throw "Out_of_stack_space";
			} catch (Genode::Native_capability::Reference_count_overflow) {
				throw "Native_capability::Reference_count_overflow";
			}
		}
	} catch (const char * ex) {
		log("created ", i, " threads before I got '", ex, "'");
		for (unsigned j = i; j > 0; j--) {
			destroy(heap, threads[j - 1]);
			threads[j - 1] = nullptr;
		}
		return;
	}

	/*
	 * We have to get a Out_of_stack_space message, because we can't create
	 * up to max threads, because already the main thread is running ...
	 */
	throw -21;
}


/*********************************
 ** Using Locks in creative ways *
 *********************************/

struct Lock_helper : Thread
{
	enum { STACK_SIZE = 0x2000 };

	Lock &lock;
	bool &lock_is_free;
	bool  unlock;

	Lock_helper(Env &env, const char * name, Cpu_session &cpu, Lock &lock,
	            bool &lock_is_free, bool unlock = false)
	:
		Thread(env, name, STACK_SIZE, Thread::Location(), Thread::Weight(),
		       cpu),
		lock(lock), lock_is_free(lock_is_free), unlock(unlock)
	{ }

	void entry()
	{
		log(" thread '", name(), "' started");

		if (unlock)
			lock.unlock();

		lock.lock();

		if (!lock_is_free) {
			log(" thread '", name(), "' got lock but somebody else is within"
			    " critical section !?");
			throw -22;
		}

		log(" thread '", name(), "' done");

		lock.unlock();
	}
};

static void test_locks(Genode::Env &env)
{
	Lock lock         (Lock::LOCKED);

	bool lock_is_free = true;

	Cpu_connection cpu_m(env, "prio middle", Cpu_session::PRIORITY_LIMIT / 4);
	Cpu_connection cpu_l(env, "prio low", Cpu_session::PRIORITY_LIMIT / 2);

	enum { SYNC_STARTUP = true };

	Lock_helper l1(env, "lock_low1", cpu_l, lock, lock_is_free);
	Lock_helper l2(env, "lock_low2", cpu_l, lock, lock_is_free);
	Lock_helper l3(env, "lock_low3", cpu_l, lock, lock_is_free);
	Lock_helper l4(env, "lock_low4", cpu_l, lock, lock_is_free, SYNC_STARTUP);

	l1.start();
	l2.start();
	l3.start();
	l4.start();

	lock.lock();

	log(" thread '", Thread::myself()->name(), "' - I'm the lock holder - "
	    "take lock again");

	/* we are within the critical section - lock is not free */
	lock_is_free = false;

	/* start another low prio thread to wake current thread when it blocks */
	Lock_helper l5(env, "lock_low5", cpu_l, lock, lock_is_free, SYNC_STARTUP);
	l5.start();

	lock.lock();
	log(" I'm the lock holder - still alive");
	lock_is_free = true;

	lock.unlock();

	/* check that really all threads come back ! */
	l1.join();
	l2.join();
	l3.join();
	l4.join();
	l5.join();

	log("running '", __func__, "' done");
}


/**********************************
 ** Using cxa guards concurrently *
 **********************************/

struct Cxa_helper : Thread
{
	enum { STACK_SIZE = 0x2000 };

	Lock &in_cxa;
	Lock &sync_startup;
	int   test;
	bool  sync;

	Cxa_helper(Env &env, const char * name, Cpu_session &cpu, Lock &cxa,
	           Lock &startup, int test, bool sync = false)
	:
		Thread(env, name, STACK_SIZE, Thread::Location(), Thread::Weight(),
		       cpu),
		in_cxa(cxa), sync_startup(startup), test(test), sync(sync)
	{ }

	void entry()
	{
		log(" thread '", name(), "' started");

		if (sync)
			sync_startup.unlock();

		struct Contention {
			Contention(Name name, Lock &in_cxa, Lock &sync_startup)
			{
				log(" thread '", name, "' in static constructor");
				sync_startup.unlock();
				in_cxa.lock();
			}
		};

		if (test == 1)
			static Contention contention (name(), in_cxa, sync_startup);
		else
		if (test == 2)
			static Contention contention (name(), in_cxa, sync_startup);
		else
		if (test == 3)
			static Contention contention (name(), in_cxa, sync_startup);
		else
		if (test == 4)
			static Contention contention (name(), in_cxa, sync_startup);
		else
			throw -25;

		log(" thread '", name(), "' done");
	}
};

static void test_cxa_guards(Env &env)
{
	log("running '", __func__, "'");

	Cpu_connection cpu_m(env, "prio middle", Cpu_session::PRIORITY_LIMIT / 4);
	Cpu_connection cpu_l(env, "prio low", Cpu_session::PRIORITY_LIMIT / 2);

	{
		enum { TEST_1ST = 1 };

		Lock in_cxa       (Lock::LOCKED);
		Lock sync_startup (Lock::LOCKED);

		/* start low priority thread */
		Cxa_helper cxa_l(env, "cxa_low", cpu_l, in_cxa, sync_startup, TEST_1ST);
		cxa_l.start();

		/* wait until low priority thread is inside static variable */
		sync_startup.lock();
		sync_startup.unlock();

		/* start high priority threads */
		Cxa_helper cxa_h1(env, "cxa_high_1", env.cpu(), in_cxa, sync_startup,
		                  TEST_1ST);
		Cxa_helper cxa_h2(env, "cxa_high_2", env.cpu(), in_cxa, sync_startup,
		                  TEST_1ST);
		Cxa_helper cxa_h3(env, "cxa_high_3", env.cpu(), in_cxa, sync_startup,
		                  TEST_1ST);
		Cxa_helper cxa_h4(env, "cxa_high_4", env.cpu(), in_cxa, sync_startup,
		                  TEST_1ST);
		cxa_h1.start();
		cxa_h2.start();
		cxa_h3.start();
		cxa_h4.start();

		/* start middle priority thread */
		enum { SYNC_STARTUP = true };
		Cxa_helper cxa_m(env, "cxa_middle", cpu_m, in_cxa, sync_startup,
		                 TEST_1ST, SYNC_STARTUP);
		cxa_m.start();

		/*
		 * high priority threads are for sure in the static Contention variable,
		 * if the middle priority thread manages to sync with current
		 * (high priority) entrypoint thread
		 */
		sync_startup.lock();

		/* let's see whether we get all our threads out of the static variable */
		in_cxa.unlock();

		/* eureka ! */
		cxa_h1.join(); cxa_h2.join(); cxa_h3.join(); cxa_h4.join();
		cxa_m.join();
		cxa_l.join();
	}

	{
		enum { TEST_2ND = 2, TEST_3RD = 3, TEST_4TH = 4 };

		Lock in_cxa_2       (Lock::LOCKED);
		Lock sync_startup_2 (Lock::LOCKED);
		Lock in_cxa_3       (Lock::LOCKED);
		Lock sync_startup_3 (Lock::LOCKED);
		Lock in_cxa_4       (Lock::LOCKED);
		Lock sync_startup_4 (Lock::LOCKED);

		/* start low priority threads */
		Cxa_helper cxa_l_2(env, "cxa_low_2", cpu_l, in_cxa_2, sync_startup_2,
		                   TEST_2ND);
		Cxa_helper cxa_l_3(env, "cxa_low_3", cpu_l, in_cxa_3, sync_startup_3,
		                   TEST_3RD);
		Cxa_helper cxa_l_4(env, "cxa_low_4", cpu_l, in_cxa_4, sync_startup_4,
		                   TEST_4TH);
		cxa_l_2.start();
		cxa_l_3.start();
		cxa_l_4.start();

		/* wait until low priority threads are inside static variables */
		sync_startup_2.lock();
		sync_startup_2.unlock();
		sync_startup_3.lock();
		sync_startup_3.unlock();
		sync_startup_4.lock();
		sync_startup_4.unlock();

		/* start high priority threads */
		Cxa_helper cxa_h1_2(env, "cxa_high_1_2", env.cpu(), in_cxa_2,
		                    sync_startup_2, TEST_2ND);
		Cxa_helper cxa_h2_2(env, "cxa_high_2_2", env.cpu(), in_cxa_2,
		                    sync_startup_2, TEST_2ND);
		Cxa_helper cxa_h3_2(env, "cxa_high_3_2", env.cpu(), in_cxa_2,
		                    sync_startup_2, TEST_2ND);
		Cxa_helper cxa_h4_2(env, "cxa_high_4_2", env.cpu(), in_cxa_2,
		                    sync_startup_2, TEST_2ND);

		Cxa_helper cxa_h1_3(env, "cxa_high_1_3", env.cpu(), in_cxa_3,
		                    sync_startup_3, TEST_3RD);
		Cxa_helper cxa_h2_3(env, "cxa_high_2_3", env.cpu(), in_cxa_3,
		                    sync_startup_3, TEST_3RD);
		Cxa_helper cxa_h3_3(env, "cxa_high_3_3", env.cpu(), in_cxa_3,
		                    sync_startup_3, TEST_3RD);
		Cxa_helper cxa_h4_3(env, "cxa_high_4_3", env.cpu(), in_cxa_3,
		                    sync_startup_3, TEST_3RD);

		Cxa_helper cxa_h1_4(env, "cxa_high_1_4", env.cpu(), in_cxa_4,
		                    sync_startup_4, TEST_4TH);
		Cxa_helper cxa_h2_4(env, "cxa_high_2_4", env.cpu(), in_cxa_4,
		                    sync_startup_4, TEST_4TH);
		Cxa_helper cxa_h3_4(env, "cxa_high_3_4", env.cpu(), in_cxa_4,
		                    sync_startup_4, TEST_4TH);
		Cxa_helper cxa_h4_4(env, "cxa_high_4_4", env.cpu(), in_cxa_4,
		                    sync_startup_4, TEST_4TH);

		cxa_h1_2.start(); cxa_h1_3.start(); cxa_h1_4.start();
		cxa_h2_2.start(); cxa_h2_3.start(); cxa_h2_4.start();
		cxa_h3_2.start(); cxa_h3_3.start(); cxa_h3_4.start();
		cxa_h4_2.start(); cxa_h4_3.start(); cxa_h4_4.start();

		/* start middle priority threads */
		enum { SYNC_STARTUP = true };
		Cxa_helper cxa_m_2(env, "cxa_middle_2", cpu_m, in_cxa_2,
		                   sync_startup_2, TEST_2ND, SYNC_STARTUP);
		Cxa_helper cxa_m_3(env, "cxa_middle_3", cpu_m, in_cxa_3,
		                   sync_startup_3, TEST_3RD, SYNC_STARTUP);
		Cxa_helper cxa_m_4(env, "cxa_middle_4", cpu_m, in_cxa_4,
		                   sync_startup_4, TEST_4TH, SYNC_STARTUP);

		cxa_m_2.start();
		cxa_m_3.start();
		cxa_m_4.start();

		/*
		 * high priority threads are for sure in the static Contention
		 * variables, if the middle priority threads manage to sync with
		 * current (high priority) entrypoint thread
		 */
		sync_startup_2.lock();
		sync_startup_3.lock();
		sync_startup_4.lock();

		/* let's see whether we get all our threads out of the static variable */
		in_cxa_4.unlock();
		in_cxa_3.unlock();
		in_cxa_2.unlock();

		cxa_h1_2.join(); cxa_h2_2.join(); cxa_h3_2.join(); cxa_h4_2.join();
		cxa_m_2.join(); cxa_l_2.join();

		cxa_h1_3.join(); cxa_h2_3.join(); cxa_h3_3.join(); cxa_h4_3.join();
		cxa_m_3.join(); cxa_l_3.join();

		cxa_h1_4.join(); cxa_h2_4.join(); cxa_h3_4.join(); cxa_h4_4.join();
		cxa_m_4.join(); cxa_l_4.join();
	}
	log("running '", __func__, "' done");
}

void Component::construct(Env &env)
{
	log("--- thread test started ---");

	Attached_rom_dataspace config(env, "config");

	try {
		test_stack_alloc(env);
		test_stack_alignment(env);
		test_main_thread();
		test_cpu_session(env);
		test_locks(env);
		test_cxa_guards(env);

		if (config.xml().has_sub_node("pause_resume"))
			test_pause_resume(env);

		test_create_as_many_threads(env);
	} catch (int error) {
		Genode::error("error ", error);
		throw;
	}

	log("--- test completed successfully ---");
}
