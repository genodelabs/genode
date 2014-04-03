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
#include <base/printf.h>
#include <base/thread.h>
#include <base/env.h>
#include <cpu_session/connection.h>


using namespace Genode;


/*********************+********************
 ** Thread-context allocator concurrency **
 ******************************************/

template <int CHILDREN>
struct Helper : Thread<0x2000>
{
	void *child[CHILDREN];

	Helper() : Thread<0x2000>("helper") { }

	void *context() const { return _context; }

	void entry()
	{
		Helper helper[CHILDREN];

		for (unsigned i = 0; i < CHILDREN; ++i)
			child[i] = helper[i].context();
	}
};


static void test_context_alloc()
{
	/*
	 * Create HELPER threads, which concurrently create CHILDREN threads each.
	 * This most likely triggers any race in the thread-context allocation.
	 */
	enum { HELPER = 10, CHILDREN = 10 };

	Helper<CHILDREN> helper[HELPER];

	for (unsigned i = 0; i < HELPER; ++i) helper[i].start();
	for (unsigned i = 0; i < HELPER; ++i) helper[i].join();

	if (0)
		for (unsigned i = 0; i < HELPER; ++i)
			for (unsigned j = 0; j < CHILDREN; ++j)
				printf("%p [%d.%d]\n", helper[i].child[j], i, j);
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

	vprintf(format, list);

	va_end(list);
}


static void log_stack_address(char const *who)
{
	long dummy;
	printf("%s stack @ %p\n", who, &dummy);
}


struct Stack_helper : Thread<0x2000>
{
	Stack_helper() : Thread<0x2000>("stack_helper") { }

	void entry()
	{
		log_stack_address("helper");
		test_stack_alignment_varargs("%f\n%g\n", 3.142, 2.718);
	}
};


static void test_stack_alignment()
{
	Stack_helper helper;

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
	/* check wether my thread object exists */
	Thread_base * myself = Genode::Thread_base::myself();
	if (!myself) { throw -1; }
	printf("thread base          %p\n", myself);

	/* check wether my stack is inside the first context region */
	addr_t const context_base = Native_config::context_area_virtual_base();
	addr_t const context_size = Native_config::context_area_virtual_size();
	addr_t const context_top  = context_base + context_size;
	addr_t const stack_top  = (addr_t)myself->stack_top();
	addr_t const stack_base = (addr_t)myself->stack_base();
	if (stack_top  <= context_base) { throw -2; }
	if (stack_top  >  context_top)  { throw -3; }
	if (stack_base >= context_top)  { throw -4; }
	if (stack_base <  context_base) { throw -5; }
	printf("thread stack top     %p\n", myself->stack_top());
	printf("thread stack bottom  %p\n", myself->stack_base());

	/* check wether my stack pointer is inside my stack */
	unsigned dummy = 0;
	addr_t const sp = (addr_t)&dummy;
	if (sp >= stack_top)  { throw -6; }
	if (sp <  stack_base) { throw -7; }
	printf("thread stack pointer %p\n", (void *)sp);
}


/******************************************
 ** Using cpu-session for thread creation *
 ******************************************/

struct Cpu_helper : Thread<0x1000>
{
	Cpu_helper(const char * name, Cpu_session * cpu)
	: Thread<0x1000>(name, cpu) { }

	void entry()
	{
		printf("%s : _cpu_session=0x%p env()->cpu_session()=0x%p\n", _context->name, _cpu_session, env()->cpu_session());
	}
};

static void test_cpu_session()
{
	Cpu_helper thread0("prio high  ", env()->cpu_session());
	thread0.start();
	thread0.join();

	Cpu_connection con1("prio middle", Cpu_session::PRIORITY_LIMIT / 4);
	Cpu_helper thread1("prio middle", &con1);
	thread1.start();
	thread1.join();

	Cpu_connection con2("prio low", Cpu_session::PRIORITY_LIMIT / 2);
	Cpu_helper thread2("prio low   ", &con2);
	thread2.start();
	thread2.join();
}

int main()
{
	try {
		test_context_alloc();
		test_stack_alignment();
		test_main_thread();
		test_cpu_session();
	} catch (int error) {
		return error;
	}
}
