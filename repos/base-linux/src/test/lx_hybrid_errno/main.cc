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

struct Thread : Genode::Thread_deprecated<STACK_SIZE>
{
	Genode::Lock &_barrier;

	Thread(Genode::Lock &barrier)
	: Genode::Thread_deprecated<STACK_SIZE>("stat"), _barrier(barrier) { start(); }

	void entry()
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
		_barrier.unlock();
	}
};


static int exit_status;
static void exit_on_suspended() { exit(exit_status); }


struct Unexpected_errno_change { };

/*
 * Component implements classical main function in construct.
 */
void Component::construct(Genode::Env &env)
{
	Genode::log("--- thread-local errno test ---");

	static Genode::Lock barrier(Genode::Lock::LOCKED);

	int const orig_errno = errno;

	Genode::log("main: before thread creation, errno=", orig_errno);

	/* create thread, which modifies its thread-local errno value */
	static Thread thread(barrier);

	/* block until the thread performed a 'stat' syscall */
	barrier.lock();

	Genode::log("main: after thread completed, errno=", errno);

	if (orig_errno != errno) {
		Genode::error("unexpected change of main thread's errno value");
		throw Unexpected_errno_change();
	}

	Genode::log("--- finished thread-local errno test ---");
	exit_status = 0;
	env.ep().schedule_suspend(exit_on_suspended, nullptr);
}
