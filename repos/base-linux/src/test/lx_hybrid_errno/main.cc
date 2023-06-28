/*
 * \brief  Test for thread-local errno handling of hybrid Linux/Genode programs
 * \author Norman Feske
 * \date   2011-12-05
 */

/* Genode includes */
#include <base/component.h>
#include <base/thread.h>
#include <base/log.h>

/* libc includes */
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#include <stdlib.h>

enum { STACK_SIZE = 4096 };

struct Thread : Genode::Thread
{
	Genode::Blockade &_barrier;

	Thread(Genode::Blockade &barrier, Genode::Env &env)
	: Genode::Thread(env, "stat", STACK_SIZE), _barrier(barrier) { start(); }

	void entry() override
	{
		/*
		 * Stat syscall should return with errno ENOENT
		 */
		struct stat buf;
		int ret = stat("", &buf);

		Genode::log("thread: stat returned ", ret, ", errno=", errno);

		/*
		 * Let main thread procees
		 */
		_barrier.wakeup();
	}
};


struct Unexpected_errno_change { };


void Component::construct(Genode::Env &env)
{
	Genode::log("--- thread-local errno test ---");

	static Genode::Blockade barrier;

	int const orig_errno = errno;

	Genode::log("main: before thread creation, errno=", orig_errno);

	/* create thread, which modifies its thread-local errno value */
	static Thread thread(barrier, env);

	/* block until the thread performed a 'stat' syscall */
	barrier.block();

	Genode::log("main: after thread completed, errno=", errno);

	if (orig_errno != errno) {
		Genode::error("unexpected change of main thread's errno value");
		throw Unexpected_errno_change();
	}

	Genode::log("--- finished thread-local errno test ---");
	env.parent().exit(0);
}
