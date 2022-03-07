#include <time.h>
#include <string.h>
#include <stdlib.h>
#include <cstdio>

struct Duration { unsigned long value; };

struct Time
{
	timespec _timespec { 0, 0 };

	Time()
	{
		clock_gettime(CLOCK_REALTIME, &_timespec);
	}

	Time(timespec timespec) : _timespec(timespec) { }

	static Duration duration(Time t1, Time t2)
	{
		auto usecs = [&] (timespec ts) {
			return 1000UL*1000UL*((unsigned long)ts.tv_sec % 1000UL)
			       + (unsigned long)ts.tv_nsec/1000UL; };

		return Duration { usecs(t2._timespec) - usecs(t1._timespec) };
	}
};

#include "common.h"


void triplet_test(void * src, void * dst, size_t size, unsigned iterations)
{
	size_t size_kb = size / 1024;

	unsigned long res1 = timed_test(src, nullptr, size, iterations, touch_words);
	unsigned long res2 = timed_test(src, src,     size, iterations, memcpy);
	unsigned long res3 = timed_test(src, dst,     size, iterations, memcpy);

	printf("%luKB (nsec/KB): %lu | %lu | %lu\n", size_kb,
	       1000*res1 / size_kb / iterations,
	       1000*res2 / size_kb / iterations,
	       1000*res3 / size_kb / iterations);
}

int main(int, char**)
{
	enum { MAX_KB = 4*1024 };

	char * buf1 = new char[MAX_KB*1024];
	char * buf2 = new char[MAX_KB*1024];

	memset(buf1, 0, MAX_KB*1024);
	memset(buf2, 0, MAX_KB*1024);

	sweep_test<8, MAX_KB>(buf1, buf2, 100, triplet_test);

	delete[] buf1;
	delete[] buf2;
}
