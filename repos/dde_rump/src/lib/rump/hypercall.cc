/**
 * \brief  Rump hypercall-interface implementation
 * \author Sebastian Sumpf
 * \author Josef Soentgen
 * \date   2013-12-06
 */

/*
 * Copyright (C) 2013-2022 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include "sched.h"

#include <base/log.h>
#include <base/sleep.h>
#include <rump/env.h>
#include <rump/timed_semaphore.h>
#include <util/allocator_fap.h>
#include <util/random.h>
#include <util/string.h>
#include <format/snprintf.h>

enum {
	SUPPORTED_RUMP_VERSION = 17,
	MAX_VIRTUAL_MEMORY = (sizeof(void *) == 4 ? 256UL : 4096UL) * 1024 * 1024
};

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
	if (version != SUPPORTED_RUMP_VERSION) {
		Genode::error("unsupported rump-kernel version (", version, ") - "
		              "supported is ", (int)SUPPORTED_RUMP_VERSION);
		return -1;
	}

	_rump_upcalls = *hyp;

	/* register context for main EP */
	main_context()->thread(Genode::Thread::myself());
	Hard_context_registry::r().insert(main_context());

	return 0;
}



/*************************
 ** Parameter retrieval **
 *************************/

static size_t _rump_memlimit = 0;


void rump_set_memlimit(Genode::size_t limit)
{
	_rump_memlimit = limit;
}


int rumpuser_getparam(const char *name, void *buf, size_t buflen)
{
	enum {
		MIN_RESERVE_MEM = 1U << 20,
		MIN_RUMP_MEM    = 6U << 20,
	};

	/* support one cpu */
	if (!Genode::strcmp(name, "_RUMPUSER_NCPU")) {
		Genode::copy_cstring((char *)buf, "1", 2);
		return 0;
	}

	/* return out cool host name */
	if (!Genode::strcmp(name, "_RUMPUSER_HOSTNAME")) {
		Genode::copy_cstring((char *)buf, "rump4genode", 12);
		return 0;
	}

	if (!Genode::strcmp(name, "RUMP_MEMLIMIT")) {

		if (!_rump_memlimit) {
			Genode::error("no RAM limit set");
			throw -1;
		}

		/*
		 * Set RAM limit and reserve a 10th or at least 1MiB for
		 * Genode meta-data.
		 */
		Genode::size_t rump_ram = _rump_memlimit;
		size_t const   reserve  = Genode::max((size_t)MIN_RESERVE_MEM, rump_ram / 10);

		if (reserve < MIN_RESERVE_MEM) {
			Genode::error("could not reserve enough RAM for meta-data, need at least ",
			              (size_t)MIN_RESERVE_MEM >> 20, " MiB");
			throw -1;
		}

		rump_ram -= reserve;

		/* check RAM limit is enough... */
		if (rump_ram < MIN_RUMP_MEM) {
			Genode::error("RAM limit too small, need at least ",
			              (size_t)MIN_RUMP_MEM >> 20, " MiB");
			throw -1;
		}

		/* ... and is in valid range (overflow) */
		if (rump_ram >= _rump_memlimit) {
			Genode::error("rump RAM limit invalid");
			throw -1;
		}

		rump_ram  = Genode::min((unsigned long)MAX_VIRTUAL_MEMORY, rump_ram);

		/* convert to string */
		Format::snprintf((char *)buf, buflen, "%zu", rump_ram);
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


typedef Allocator::Fap<MAX_VIRTUAL_MEMORY, Allocator_policy> Rump_alloc;

static Genode::Mutex & alloc_mutex()
{
	static Genode::Mutex inst { };
	return inst;
}


static Rump_alloc* allocator()
{
	static Rump_alloc _fap(true);
	return &_fap;
}


int rumpuser_malloc(size_t len, int alignment, void **memp)
{
	Genode::Mutex::Guard guard(alloc_mutex());

	int align = alignment ? Genode::log2(alignment) : 0;
	*memp     = allocator()->alloc(len, align);

	return *memp ? 0 : -1;
}


void rumpuser_free(void *mem, size_t len)
{
	Genode::Mutex::Guard guard(alloc_mutex());

	allocator()->free(mem, len);
}


/************
 ** Clocks **
 ************/

int rumpuser_clock_gettime(int enum_rumpclock, int64_t *sec, long *nsec)
{
	Genode::uint64_t t = Rump::env().timer().elapsed_ms();
	*sec = (int64_t)t / 1000;
	*nsec = (t % 1000) * 1000 * 1000;
	return 0;
}


int rumpuser_clock_sleep(int enum_rumpclock, int64_t sec, long nsec)
{
	int nlocks;
	Genode::uint64_t msec = 0;

	rumpkern_unsched(&nlocks, 0);
	switch (enum_rumpclock) {
		case RUMPUSER_CLOCK_RELWALL:
			msec = (Genode::uint64_t)sec * 1000 + nsec / (1000*1000UL);
			break;
		case RUMPUSER_CLOCK_ABSMONO:
			Genode::uint64_t target_abs_msec =
				(((Genode::uint64_t)sec * 1000) +
				 ((Genode::uint64_t)nsec / (1000 * 1000)));
			Genode::uint64_t current_abs_msec = Rump::env().timer().elapsed_ms();

			if (target_abs_msec > current_abs_msec)
				msec = target_abs_msec - current_abs_msec;

			break;
	}

	/*
	 * When using rump in the form of vfs_rump as file-system implementation,
	 * this function is steadily called with sleep times in the range of 0 to
	 * 10 ms, inducing load even when the file system is not accessed.
	 *
	 * This load on idle can be lowered by forcing a sleep time of at least one
	 * second. This does not harm the operation of vfs_rump because the file
	 * system is not driven by time.
	 */
	if (msec < 1000)
		msec = 1000;

	Rump::env().sleep_sem().down(true, Genode::Microseconds(msec * 1000));

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
