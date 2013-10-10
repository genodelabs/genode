/*
 * \brief  Test for thread-local errno handling of hybrid Linux/Genode programs
 * \author Norman Feske
 * \date   2011-12-05
 */

/* Genode includes */
#include <base/thread.h>
#include <base/printf.h>

/* libc includes */
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>

enum { STACK_SIZE = 4096 };

struct Thread : Genode::Thread<STACK_SIZE>
{
	Genode::Lock &_barrier;

	Thread(Genode::Lock &barrier)
	: Genode::Thread<STACK_SIZE>("stat"), _barrier(barrier) { start(); }

	void entry()
	{
		/*
		 * Stat syscall should return with errno ENOENT
		 */
		struct stat buf;
		int ret = stat("", &buf);

		Genode::printf("thread: stat returned %d, errno=%d\n", ret, errno);

		/*
		 * Let main thread procees
		 */
		_barrier.unlock();
	}
};


int main(int, char **)
{
	Genode::printf("--- thread-local errno test ---\n");

	static Genode::Lock barrier(Genode::Lock::LOCKED);

	int const orig_errno = errno;

	Genode::printf("main: before thread creation, errno=%d\n", orig_errno);

	/* create thread, which modifies its thread-local errno value */
	static Thread thread(barrier);

	/* block until the thread performed a 'stat' syscall */
	barrier.lock();

	Genode::printf("main: after thread completed, errno=%d\n", errno);

	if (orig_errno != errno) {
		PERR("unexpected change of main thread's errno value");
		return -1;
	}

	Genode::printf("--- finished thread-local errno test ---\n");
	return 0;
}
