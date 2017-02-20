/*
 * \brief  Linux kernel API
 * \author Norman Feske
 * \author Sebastian Sumpf
 * \author Josef Soentgen
 * \date   2014-08-21
 *
 * Based on the prototypes found in the Linux kernel's 'include/'.
 */

/*
 * Copyright (C) 2014-2017 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

/******************
 ** linux/time.h **
 ******************/

struct timespec {
	__kernel_time_t tv_sec;
	long            tv_nsec;
};

struct timeval
{
	__kernel_time_t      tv_sec;
	__kernel_suseconds_t tv_usec;
};

struct timespec current_kernel_time(void);
void do_gettimeofday(struct timeval *tv);

#define CURRENT_TIME (current_kernel_time())



enum {
	CLOCK_REALTIME  = 0,
	CLOCK_MONOTONIC = 1,
	NSEC_PER_USEC   = 1000L,
	NSEC_PER_MSEC   = 1000000L,
	NSEC_PER_SEC    = 1000L * NSEC_PER_MSEC,
};


/*******************
 ** linux/ktime.h **
 *******************/

union ktime { s64 tv64; };

typedef union ktime ktime_t;

ktime_t ktime_add_ns(const ktime_t kt, u64 nsec);

static inline ktime_t ktime_add_us(const ktime_t kt, const u64 usec)
{
	return ktime_add_ns(kt, usec * 1000);
}

static inline ktime_t ktime_get(void)
{
	return (ktime_t){ .tv64 = (s64)jiffies * HZ * NSEC_PER_MSEC /* ns */ };
}

static inline ktime_t ktime_set(const long sec, const unsigned long nsec)
{
	return (ktime_t){ .tv64 = (s64)sec * NSEC_PER_SEC + (s64)nsec /* ns */ };
}

static inline ktime_t ktime_add(const ktime_t a, const ktime_t b)
{
	return (ktime_t){ .tv64 = a.tv64 + b.tv64 /* ns */ };
}


s64 ktime_us_delta(const ktime_t later, const ktime_t earlier);

static inline struct timeval ktime_to_timeval(const ktime_t kt)
{
	struct timeval tv;
	tv.tv_sec = kt.tv64 / NSEC_PER_SEC;
	tv.tv_usec = (kt.tv64 - (tv.tv_sec * NSEC_PER_SEC)) / NSEC_PER_USEC;
	return tv;
}

ktime_t ktime_get_real(void);
ktime_t ktime_sub(const ktime_t, const ktime_t);
ktime_t ktime_get_monotonic_offset(void);

