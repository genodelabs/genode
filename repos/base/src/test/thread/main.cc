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
#include <base/attached_rom_dataspace.h>
#include <base/component.h>
#include <base/heap.h>
#include <base/log.h>
#include <base/rpc_server.h>
#include <base/rpc_client.h>
#include <base/thread.h>
#include <util/reconstructible.h>
#include <cpu_session/connection.h>
#include <cpu_thread/client.h>
#include <cpu/memory_barrier.h>

/* compiler includes */
#include <stdarg.h>

using namespace Genode;


static constexpr Thread::Stack_size STACK_SIZE = { 0x3000 };


/*********************************
 ** Stack-allocator concurrency **
 *********************************/

template <int CHILDREN>
class Helper : Thread
{
	private:

		/*
		 * Noncopyable
		 */
		Helper(Helper const &);
		Helper &operator = (Helper const &);

	public:

		using Thread::start;
		using Thread::join;

		void *child[CHILDREN];

		Env &_env;

		Helper(Env &env) : Thread(env, "helper", STACK_SIZE), _env(env) { }

		void *stack() const
		{
			return info().convert<void *>(
				[&] (Stack_info info) { return (void *)info.top; },
				[&] (Stack_error)     { return nullptr; });
		}

		void entry() override
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
	Stack_helper(Env &env) : Thread(env, "stack_helper", STACK_SIZE) { }

	void entry() override
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

	addr_t const stack_top  = Thread::mystack().top;
	addr_t const stack_base = Thread::mystack().base;

	if (stack_top  <= stack_slot_base) { throw -2; }
	if (stack_top  >  stack_slot_top)  { throw -3; }
	if (stack_base >= stack_slot_top)  { throw -4; }
	if (stack_base <  stack_slot_base) { throw -5; }

	log("thread stack top     ", (void *)stack_top);
	log("thread stack bottom  ", (void *)stack_base);

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
	Env &_env;

	Cpu_helper(Env &env, Name const &name)
	:
		Thread(env, name, STACK_SIZE, Thread::Location()),
		_env(env)
	{ }

	void entry() override { log(Thread::name); }
};


struct Pause_helper : Thread
{
	volatile unsigned loop = 0;
	volatile bool beep = false;

	Pause_helper(Env &env, Name const &name)
	: Thread(env, name, STACK_SIZE, Thread::Location()) { }

	void entry() override
	{
		while (1) {
			/*
			 * Don't log here, since this thread becomes "paused".
			 * If it is holding the lock of the log backend being paused, all
			 * other threads of this task trying to print log messages will
			 * block - looks like a deadlock.
			 */
			loop = loop + 1;
			if (beep) {
				log("beep");
				beep = false;
				loop = loop + 1;
				return;
			}
		}
	}
};


static void test_pause_resume(Env &env)
{
	log("running '", __func__, "'");

	Pause_helper thread(env, "pause");
	thread.start();

	while (thread.loop < 1) { }

	Thread_state state { };
	Cpu_thread_client thread_client(thread.cap());

	log("--- pausing ---");
	thread_client.pause();
	unsigned loop_paused = thread.loop;
	log("--- paused ---");

	log("--- reading thread state ---");
	state = thread_client.state();
	if (state.state == Thread_state::State::UNAVAILABLE)
		throw -10;

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

	Heap heap(env.ram(), env.rm());

	unsigned i = 0;
	bool denied = false;
	bool out_of_stack_space = false;
	for (; i < max; i++) {
		threads[i] = new (heap) Cpu_helper(env, Thread::Name(i + 1));

		if (threads[i]->info() == Thread::Stack_error::STACK_AREA_EXHAUSTED) {
			out_of_stack_space = true;
			break;
		}

		if (threads[i]->start() == Thread::Start_result::DENIED) {
			denied = true;
			break;
		}
		threads[i]->join();
	}

	for (unsigned j = i; j > 0; j--) {
		destroy(heap, threads[j - 1]);
		threads[j - 1] = nullptr;
	}

	if (denied) {
		log("created ", i, " threads before thread creation got denied");
		return;
	}

	/*
	 * We have to get a Out_of_stack_space message, because we can't create
	 * up to max threads, because already the main thread is running ...
	 */
	if (!out_of_stack_space)
		throw -21;
}


/*********************************
 ** Using Locks in creative ways *
 *********************************/

struct Lock_helper : Thread
{
	Blockade &lock;
	bool     &lock_is_free;
	bool      unlock;

	Lock_helper(Env &env, const char * name, Cpu_session &, Blockade &lock,
	            bool &lock_is_free, bool unlock = false)
	:
		Thread(env, name, STACK_SIZE, Thread::Location()),
		lock(lock), lock_is_free(lock_is_free), unlock(unlock)
	{ }

	void entry() override
	{
		log(" thread '", name, "' started");

		if (unlock)
			lock.wakeup();

		lock.block();

		if (!lock_is_free) {
			log(" thread '", name, "' got lock but somebody else is within"
			    " critical section !?");
			throw -22;
		}

		log(" thread '", name, "' done");

		lock.wakeup();
	}
};


/*********************************************
 ** Successive construction and destruction **
 *********************************************/

struct Create_destroy_helper : Thread
{
	unsigned const    result_value;
	unsigned volatile result { ~0U };

	Create_destroy_helper(Env &env, unsigned result_value)
	: Thread(env, "create_destroy", STACK_SIZE),
	  result_value(result_value)
	{ }

	void entry() override
	{
		result = result_value;
	}
};

static void test_successive_create_destroy_threads(Env &env)
{
	log("running '", __func__, "'");

	for (unsigned i = 0; i < 500; i++) {
		Create_destroy_helper thread(env, i);
		thread.start();
		thread.join();
		if (thread.result != i)
			throw -30;
	}
}

/******************************************************
 ** Test destruction of inter-dependent CPU sessions **
 ******************************************************/

static void test_destroy_dependent_cpu_sessions(Env &env)
{
	log("destroy dependent CPU sessions in wrong order");

	Cpu_connection grandchild { env };
	Cpu_connection child      { env };

	/* when leaving the scope, 'child' is destructed before 'grandchild' */
}


/************************************************
 ** Test release of extended stack allocations **
 ************************************************/

static void test_free_expanded_stack(Env &env)
{
	struct Thread_with_expanded_stack : Thread
	{
		Thread_with_expanded_stack(Env &env)
		: Thread(env, "expanded", Stack_size { 16*1024 })
		{ start(); join(); }

		void entry() override
		{
			for (unsigned i = 2; i < 7; i++) {
				size_t const kb = i*16;
				Stack_size_result const result = stack_size(kb*1024);
				/* up to four stack mappings are supported */
				if (i <= 4) {
					if (result.failed()) {
						error("stack expansion unexpectedly failed");
						throw -35;
					}
				} else {
					if (result.ok()) {
						error("stack expansion unexpectedly succeeded");
						throw -36;
					}
				}
			}
		}
	};

	for (unsigned i = 0; i < 20; i++) {
		Thread_with_expanded_stack thread(env);
	}
}


void Component::construct(Env &env)
{
	log("--- thread test started ---");

	Attached_rom_dataspace config(env, "config");

	try {

		test_destroy_dependent_cpu_sessions(env);

		test_stack_alloc(env);
		test_stack_alignment(env);
		test_main_thread();
		if (config.xml().attribute_value("pause_resume", false))
			test_pause_resume(env);

		test_create_as_many_threads(env);
		test_successive_create_destroy_threads(env);
		test_free_expanded_stack(env);

	} catch (int error) {
		Genode::error("error ", error);
		throw;
	}

	log("--- test completed successfully ---");
}
