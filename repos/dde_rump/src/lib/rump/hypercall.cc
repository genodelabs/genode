/**
 * \brief  Rump hypercall-interface implementation
 * \author Sebastian Sumpf
 * \author Josef Soentgen
 * \date   2013-12-06
 */

/*
 * Copyright (C) 2013-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include "sched.h"

#include <base/log.h>
#include <base/sleep.h>
#include <os/timed_semaphore.h>
#include <rump/env.h>
#include <util/allocator_fap.h>
#include <util/random.h>
#include <util/string.h>

extern "C" void wait_for_continue();
enum { SUPPORTED_RUMP_VERSION = 17 };

static bool verbose = false;

/* upcalls to rump kernel */
struct rumpuser_hyperup _rump_upcalls;


/*************
 ** Threads **
 *************/

static Hard_context *main_context()
{
	static Hard_context inst(0);
	return &inst;
}


static Hard_context *myself()
{
	Hard_context *h = Hard_context_registry::r().find(Genode::Thread::myself());

	if (!h)
		Genode::error("Hard context is nullptr (", Genode::Thread::myself(), ")");

	return h;
}


Timer::Connection &Hard_context::timer()
{
	static Timer::Connection _timer { Rump::env().env() };
	return _timer;
}


void rumpuser_curlwpop(int enum_rumplwpop, struct lwp *l)
{
	Hard_context *h = myself();
	switch (enum_rumplwpop) {
		case RUMPUSER_LWP_CREATE:
		case RUMPUSER_LWP_DESTROY:
			break;
		case RUMPUSER_LWP_SET:
			h->set_lwp(l);
			break;
		case RUMPUSER_LWP_CLEAR:
			h->set_lwp(0);
			break;
	}
}


struct lwp * rumpuser_curlwp(void)
{
	return myself()->get_lwp();
}


int rumpuser_thread_create(func f, void *arg, const char *name,
                           int mustjoin, int priority, int cpui_dx, void **cookie)
{
	static long count = 0;

	if (mustjoin)
		*cookie = (void *)++count;

	new (Rump::env().heap()) Hard_context_thread(name, f, arg, mustjoin ? count : 0);

	return 0;
}


void rumpuser_thread_exit()
{
	Genode::sleep_forever();
}


int errno;
void rumpuser_seterrno(int e) { errno = e; }


/********************
 ** Initialization **
 ********************/

int rumpuser_init(int version, const struct rumpuser_hyperup *hyp)
{
	Genode::log("RUMP ver: ", version);
	if (version != SUPPORTED_RUMP_VERSION) {
		Genode::error("unsupported rump-kernel version (", version, ") - "
		              "supported is ", (int)SUPPORTED_RUMP_VERSION);
		return -1;
	}

	_rump_upcalls = *hyp;

	/* register context for main EP */
	main_context()->thread(Genode::Thread::myself());
	Hard_context_registry::r().insert(main_context());

	/*
	 * Start 'Timeout_thread' so it does not get constructed concurrently (which
	 * causes one thread to spin in cxa_guard_aqcuire), making emulation *really*
	 * slow
	 */
	Genode::Timeout_thread::alarm_timer();

	return 0;
}



/*************************
 ** Parameter retrieval **
 *************************/

int rumpuser_getparam(const char *name, void *buf, size_t buflen)
{
	enum { RESERVE_MEM = 2U * 1024 * 1024 };

	/* support one cpu */
	Genode::log(name);
	if (!Genode::strcmp(name, "_RUMPUSER_NCPU")) {
		Genode::strncpy((char *)buf, "1", 2);
		return 0;
	}

	/* return out cool host name */
	if (!Genode::strcmp(name, "_RUMPUSER_HOSTNAME")) {
		Genode::strncpy((char *)buf, "rump4genode", 12);
		return 0;
	}

	if (!Genode::strcmp(name, "RUMP_MEMLIMIT")) {

		/* leave 2 MB for the Genode */
		size_t rump_ram =  Rump::env().env().ram().avail_ram().value;

		if (rump_ram <= RESERVE_MEM) {
			Genode::error("insufficient quota left: ",
			              rump_ram, " < ", (long)RESERVE_MEM, " bytes");
			return -1;
		}

		rump_ram -= RESERVE_MEM;

		/* convert to string */
		Genode::snprintf((char *)buf, buflen, "%zu", rump_ram);
		Genode::log("asserting rump kernel ", rump_ram / 1024, " KB of RAM");
		return 0;
	}

	return -1;
}


/*************
 ** Console **
 *************/

void rumpuser_putchar(int ch)
{
	enum { BUF_SIZE = 256 };
	static unsigned char buf[BUF_SIZE];
	static int count = 0;

	if (count < BUF_SIZE - 1 && ch != '\n')
		buf[count++] = (unsigned char)ch;

	if (ch == '\n' || count == BUF_SIZE - 1) {
		buf[count] = 0;
		int nlocks;
		if (myself() != main_context())
			rumpkern_unsched(&nlocks, 0);

		Genode::log("rump: ", Genode::Cstring((char const *)buf));

		if (myself() != main_context())
			rumpkern_sched(nlocks, 0);

		count = 0;
	}
}


/************
 ** Memory **
 ************/

struct Allocator_policy
{
	static int block()
	{
		int nlocks;

		if (myself() != main_context())
			rumpkern_unsched(&nlocks, 0);
		return nlocks;
	}

	static void unblock(int nlocks)
	{
		if (myself() != main_context())
			rumpkern_sched(nlocks, 0);
	}
};


typedef Allocator::Fap<128 * 1024 * 1024, Allocator_policy> Rump_alloc;

static Genode::Lock & alloc_lock()
{
	static Genode::Lock inst;
	return inst;
}


static Rump_alloc* allocator()
{
	static Rump_alloc _fap(true);
	return &_fap;
}


int rumpuser_malloc(size_t len, int alignment, void **memp)
{
	Genode::Lock::Guard guard(alloc_lock());

	int align = alignment ? Genode::log2(alignment) : 0;
	*memp     = allocator()->alloc(len, align);

	if (verbose)
		Genode::log("ALLOC: p: ", *memp, ", s: ", len, ", a: ", align, " ", alignment);


	return *memp ? 0 : -1;
}


void rumpuser_free(void *mem, size_t len)
{
	Genode::Lock::Guard guard(alloc_lock());

	allocator()->free(mem, len);

	if (verbose)
		Genode::warning("FREE: p: ", mem, ", s: ", len);
}


/************
 ** Clocks **
 ************/

int rumpuser_clock_gettime(int enum_rumpclock, int64_t *sec, long *nsec)
{
	Hard_context *h = myself();
	unsigned long t = h->timer().elapsed_ms();
	*sec = (int64_t)t / 1000;
	*nsec = (t % 1000) * 1000;
	return 0;
}


int rumpuser_clock_sleep(int enum_rumpclock, int64_t sec, long nsec)
{
	int nlocks;
	unsigned int msec = 0;

	Timer::Connection &timer  = myself()->timer();

	rumpkern_unsched(&nlocks, 0);
	switch (enum_rumpclock) {
		case RUMPUSER_CLOCK_RELWALL:
			msec = sec * 1000 + nsec / (1000*1000UL);
			break;
		case RUMPUSER_CLOCK_ABSMONO:
			msec = timer.elapsed_ms();
			msec = ((sec * 1000) + (nsec / (1000 * 1000))) - msec;
			break;
	}

	timer.msleep(msec);
	rumpkern_sched(nlocks, 0);
	return 0;
}


/*****************
 ** Random pool **
 *****************/

int rumpuser_getrandom(void *buf, size_t buflen, int flags, size_t *retp)
{
	/*
	 * Cast retp to Genode::size_t to prevent compiler error because
	 * the type of rump's size_t is int on 32 bit and long 64 bit archs.
	 */
	return rumpuser_getrandom_backend(buf, buflen, flags,
	                                  (Genode::size_t *)retp);
}


/**********
 ** Exit **
 **********/

void genode_exit(int) __attribute__((noreturn));

void rumpuser_exit(int status)
{
	if (status == RUMPUSER_PANIC)
		Genode::error("Rump panic");

	genode_exit(status);
}
