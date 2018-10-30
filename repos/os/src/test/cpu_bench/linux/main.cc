#include <time.h>
#include <stdio.h>

#include <bogomips.h>

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


int main(int, char**)
{
	printf("bogomips test:\n");
	Time s;
	bogomips();
	{
		Time e;
		Duration duration = Time::duration(s, e);
		printf("2G bogus instructions in %ld msecs (%f BogoMIPS)\n",
		       duration.usecs/1000, 2000000000 / (float)duration.usecs);
	}
}
