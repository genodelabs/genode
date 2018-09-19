#include <time.h>
#include <string.h>

#include "memcpy.h"

struct Duration { unsigned long usecs; };


struct Time
{
	timespec _timespec { 0, 0 };

	Time()
	{
		clock_gettime(CLOCK_REALTIME, &_timespec);
	}

	Time(timespec timespec) : _timespec(timespec) { }

	void print() const
	{
		printf("secs=%ld nsecs=%ld\n",
		       (long)_timespec.tv_sec, (long)_timespec.tv_nsec);
	}

	static Duration duration(Time t1, Time t2)
	{
		auto usecs = [&] (timespec ts) {
			return 1000UL*1000UL*((unsigned long)ts.tv_sec % 1000UL)
			       + (unsigned long)ts.tv_nsec/1000UL; };

		return Duration { usecs(t2._timespec) - usecs(t1._timespec) };
	}
};


struct Test {

	Time s { };

	void start() { }

	void finished()
	{
		Time e;
		Duration duration = Time::duration(s, e);

		printf("copied %ld KiB in %ld usecs ",
		       (unsigned long)TOTAL_MEM_KB, duration.usecs);
		printf("(%ld MiB/sec)\n", (unsigned long)
		       ((float)(TOTAL_MEM_KB/1024)/((float)duration.usecs/1000000)));
	}
};


struct Bytewise_test : Test
{
	void copy(void *dst, const void *src, size_t size) {
		bytewise_memcpy(dst, src, size); }
};


struct Libc_memcpy_test : Test
{
	void copy(void *dst, const void *src, size_t size) {
		memcpy(dst, src, size); }
};


struct Libc_memset_test : Test
{
	void copy(void *dst, const void *, size_t size) {
		memset(dst, 0, size); }
};


int main(int, char**)
{
	printf("bytewise memcpy test:\n");
	memcpy_test<Bytewise_test>();
	printf("libc memcpy test:\n");
	memcpy_test<Libc_memcpy_test>();
	printf("libc memset test:\n");
	memcpy_test<Libc_memset_test>();
}
