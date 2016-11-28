/*
 * \brief  Testing thread library
 * \author Alexander Boettcher
 * \author Christian Helmuth
 * \date   2013-12-13
 */

/*
 * Copyright (C) 2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <base/log.h>
#include <base/thread.h>
#include <base/component.h>
#include <base/heap.h>
#include <base/attached_rom_dataspace.h>
#include <util/volatile_object.h>
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
		Lazy_volatile_object<Helper> helper[CHILDREN];

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

	Lazy_volatile_object<Helper<CHILDREN> > helper[HELPER];

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

	Cpu_connection con1("prio middle", Cpu_session::PRIORITY_LIMIT / 4);
	Cpu_helper thread1(env, "prio middle", con1);
	thread1.start();
	thread1.join();

	Cpu_connection con2("prio low", Cpu_session::PRIORITY_LIMIT / 2);
	Cpu_helper thread2(env, "prio low   ", con2);
	thread2.start();
	thread2.join();
}


struct Pause_helper : Thread
{
	volatile unsigned loop = 0;
	volatile bool beep = false;

	enum { STACK_SIZE = 0x1000 };

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


void Component::construct(Env &env)
{
	log("--- thread test started ---");

	Attached_rom_dataspace config(env, "config");

	try {
		test_stack_alloc(env);
		test_stack_alignment(env);
		test_main_thread();
		test_cpu_session(env);

		if (config.xml().has_sub_node("pause_resume"))
			test_pause_resume(env);

		test_create_as_many_threads(env);
	} catch (int error) {
		Genode::error("error ", error);
		throw;
	}

	log("--- test completed successfully ---");
}
