/**
 * \brief  Rump hypercall-interface implementation
 * \author Sebastian Sumpf
 * \author Josef Soentgen
 * \date   2013-12-06
 */

/*
 * Copyright (C) 2013-2014 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#include "sched.h"

#include <base/env.h>
#include <base/printf.h>
#include <base/sleep.h>
#include <os/timed_semaphore.h>
#include <util/allocator_fap.h>
#include <util/string.h>

extern "C" {
	namespace Jitter {
		#include <jitterentropy.h>
	}
}

extern "C" void wait_for_continue();
enum { SUPPORTED_RUMP_VERSION = 17 };

static bool verbose = false;

/* upcalls to rump kernel */
struct rumpuser_hyperup _rump_upcalls;


/***********************************
 ** Jitter entropy for randomness **
 ***********************************/

struct Entropy
{
	struct Jitter::rand_data *ec_stir;

	Entropy()
	{
		Jitter::jent_entropy_init();
		ec_stir = Jitter::jent_entropy_collector_alloc(0, 0);
	}

	static Entropy *e()
	{
		static Entropy _e;
		return &_e;
	}

	size_t read(char *buf, size_t len)
	{
		int err;
		if ((err = Jitter::jent_read_entropy(ec_stir, buf, len) < 0)) {
			PERR("Failed to read entropy: %d", err);
			return 0;
		}

		return len;
	}
};

/********************
 ** Initialization **
 ********************/

int rumpuser_init(int version, const struct rumpuser_hyperup *hyp)
{
	PDBG("RUMP ver: %d", version);
	if (version != SUPPORTED_RUMP_VERSION) {
		PERR("Unsupported rump-kernel version (%d) - supported is %d)",
		     version, SUPPORTED_RUMP_VERSION);
		return -1;
	}

	_rump_upcalls = *hyp;

	/*
	 * Start 'Timeout_thread' so it does not get constructed concurrently (which
	 * causes one thread to spin in cxa_guard_aqcuire), making emulation *really*
	 * slow
	 */
	Genode::Timeout_thread::alarm_timer();

	/* initialize jitter entropy */
	Entropy::e();

	return 0;
}


/*************
 ** Threads **
 *************/

static Hard_context _main_thread(0);

static Hard_context *myself()
{
	Hard_context *h = dynamic_cast<Hard_context *>(Genode::Thread_base::myself());
	return h ? h : &_main_thread;
}


Timer::Connection *Hard_context::timer()
{
	static Timer::Connection _timer;
	return &_timer;
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

	new (Genode::env()->heap()) Hard_context_thread(name, f, arg, mustjoin ? count : 0);

	return 0;
}


void rumpuser_thread_exit()
{
	Genode::sleep_forever();
}


int errno;
void rumpuser_seterrno(int e) { errno = e; }


/*************************
 ** Parameter retrieval **
 *************************/

int rumpuser_getparam(const char *name, void *buf, size_t buflen)
{
	enum { RESERVE_MEM = 2U * 1024 * 1024 };

	/* support one cpu */
	PDBG("%s", name);
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
		size_t rump_ram =  Genode::env()->ram_session()->avail();

		if (rump_ram <= RESERVE_MEM) {
			PERR("Insufficient quota need left: %zu < %u bytes", rump_ram, RESERVE_MEM);
			return -1;
		}

		rump_ram -= RESERVE_MEM;

		/* convert to string */
		Genode::snprintf((char *)buf, buflen, "%zu", rump_ram);
		PERR("Asserting rump kernel %zu KB of RAM", rump_ram / 1024);
		return 0;
	}

	return -1;
}


/*************
 ** Console **
 *************/

void rumpuser_putchar(int ch)
{
	static unsigned char buf[256];
	static int count = 0;

	buf[count++] = (unsigned char)ch;

	if (ch == '\n') {
		buf[count] = 0;
		int nlocks;
		if (myself() != &_main_thread)
			rumpkern_unsched(&nlocks, 0);

		PLOG("rump: %s", buf);

		if (myself() != &_main_thread)
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

		if (myself() != &_main_thread)
			rumpkern_unsched(&nlocks, 0);
		return nlocks;
	}

	static void unblock(int nlocks)
	{
		if (myself() != &_main_thread)
			rumpkern_sched(nlocks, 0);
	}
};


typedef Allocator::Fap<128 * 1024 * 1024, Allocator_policy> Rump_alloc;
static Genode::Lock _alloc_lock;

static Rump_alloc* allocator()
{
	static Rump_alloc _fap(true);
	return &_fap;
}

int rumpuser_malloc(size_t len, int alignment, void **memp)
{
	Genode::Lock::Guard guard(_alloc_lock);

	int align = Genode::log2(alignment);
	*memp     = allocator()->alloc(len, align);

	if (verbose)
		PWRN("ALLOC: p: %p, s: %zx, a: %d", *memp, len, align);


	return *memp ? 0 : -1;
}


void rumpuser_free(void *mem, size_t len)
{
	Genode::Lock::Guard guard(_alloc_lock);

	allocator()->free(mem, len);

	if (verbose)
		PWRN("FREE: p: %p, s: %zx", mem, len);
}


/************
 ** Clocks **
 ************/

int rumpuser_clock_gettime(int enum_rumpclock, int64_t *sec, long *nsec)
{
	Hard_context *h = myself();
	unsigned long t = h->timer()->elapsed_ms();
	*sec = (int64_t)t / 1000;
	*nsec = (t % 1000) * 1000;
	return 0;
}


int rumpuser_clock_sleep(int enum_rumpclock, int64_t sec, long nsec)
{
	int nlocks;
	unsigned int msec = 0;

	Timer::Connection *timer  = myself()->timer();

	rumpkern_unsched(&nlocks, 0);
	switch (enum_rumpclock) {
		case RUMPUSER_CLOCK_RELWALL:
			msec = sec * 1000 + nsec / (1000*1000UL);
			break;
		case RUMPUSER_CLOCK_ABSMONO:
			msec = timer->elapsed_ms();
			msec = ((sec * 1000) + (nsec / (1000 * 1000))) - msec;
			break;
	}

	timer->msleep(msec);
	rumpkern_sched(nlocks, 0);
	return 0;
}


/*****************
 ** Random pool **
 *****************/

int rumpuser_getrandom(void *buf, size_t buflen, int flags, size_t *retp)
{
	*retp = Entropy::e()->read((char *)buf, buflen);
	return 0;
}


/**********
 ** Exit **
 **********/

void genode_exit(int) __attribute__((noreturn));

void rumpuser_exit(int status)
{
	if (status == RUMPUSER_PANIC)
		PERR("Rump panic");

	genode_exit(status);
}
