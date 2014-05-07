/*
 * \brief  Thread facility
 * \author Christian Helmuth
 * \date   2008-10-20
 *
 */

/*
 * Copyright (C) 2008-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#include <base/env.h>
#include <base/sleep.h>
#include <base/lock.h>
#include <base/printf.h>

extern "C" {
#include <dde_kit/thread.h>
#include <dde_kit/timer.h>
}

#include "thread.h"

using namespace Genode;


/***********+**************
 ** Thread info database **
 **************************/

class Thread_info_database : public Avl_tree<Dde_kit::Thread_info>
{
	private:

		Lock _lock;

	public:

		Dde_kit::Thread_info *lookup(Thread_base *thread_base)
		{
			Lock::Guard lock_guard(_lock);

			if (!first()) throw Dde_kit::Thread_info::Not_found();

			return first()->lookup(thread_base);
		}

		void insert(Dde_kit::Thread_info *info)
		{
			Lock::Guard lock_guard(_lock);

			Avl_tree<Dde_kit::Thread_info>::insert(info);
		}

		void remove(Dde_kit::Thread_info *info)
		{
			Lock::Guard lock_guard(_lock);

			Avl_tree<Dde_kit::Thread_info>::remove(info);
		}
};


static Thread_info_database *threads()
{
	static Thread_info_database _threads;
	return &_threads;
}


/********************************
 ** Generic thread information **
 ********************************/

unsigned Dde_kit::Thread_info::_id_counter = 0x1000;


bool Dde_kit::Thread_info::higher(Thread_info *info)
{
	return (info->_thread_base >= _thread_base);
}


Dde_kit::Thread_info *Dde_kit::Thread_info::lookup(Thread_base *thread_base)
{
	Dde_kit::Thread_info *info = this;

	do {
		if (thread_base == info->_thread_base) return info;

		if (thread_base < info->_thread_base)
			info = info->child(LEFT);
		else
			info = info->child(RIGHT);
	} while (info);

	/* thread not in AVL tree */
	throw Not_found();
}


Dde_kit::Thread_info::Thread_info(Thread_base *thread_base, const char *name)
: _thread_base(thread_base), _name(name), _id(_id_counter++)
{ }


Dde_kit::Thread_info::~Thread_info()
{ }


/********************
 ** DDE Kit thread **
 ********************/

static Dde_kit::Thread_info *adopt_thread(Thread_base *thread, const char *name)
{
	Dde_kit::Thread_info *info = 0;
	try {
		info = new (env()->heap()) Dde_kit::Thread_info(thread, name);
		threads()->insert(info);
	} catch (...) {
		PERR("thread adoption failed");
		return 0;
	}
	return info;
}


struct dde_kit_thread : public Dde_kit::Thread_info
{
	/**
	 * Dummy constructor
	 *
	 * This constructor is never executed because we only use pointers to
	 * 'dde_kit_thread'. Even though, this constructor is never used, gcc-3.4
	 * (e.g., used for OKL4v2) would report an error if it did not exist.
	 */
	dde_kit_thread() : Dde_kit::Thread_info(0, 0) { }
};


class _Thread : public Dde_kit::Thread
{
	private:

		void                (*_thread_fn)(void *);
		void                 *_thread_arg;
		Dde_kit::Thread_info *_thread_info;

	public:

		_Thread(const char *name, void (*thread_fn)(void *), void *thread_arg)
		:
			Dde_kit::Thread(name),
			_thread_fn(thread_fn), _thread_arg(thread_arg),
			_thread_info(adopt_thread(this, name))
		{ start(); }

		void entry() { _thread_fn(_thread_arg); }

		Dde_kit::Thread_info *thread_info() { return _thread_info; }
};


extern "C" struct dde_kit_thread *dde_kit_thread_create(void (*fun)(void *),
                                                        void *arg, const char *name)
{
	_Thread *thread;

	try {
		thread = new (env()->heap()) _Thread(name, fun, arg);
	} catch (...) {
		PERR("thread creation failed");
		return 0;
	}

	return static_cast<struct dde_kit_thread *>(thread->thread_info());
}


extern "C" struct dde_kit_thread *dde_kit_thread_adopt_myself(const char *name)
{
	try {
		return static_cast<dde_kit_thread *>(adopt_thread(Thread_base::myself(), name));
	} catch (...) {
		PERR("thread adoption failed");
		return 0;
	}
}


extern "C" struct dde_kit_thread *dde_kit_thread_myself()
{
	try {
		return static_cast<struct dde_kit_thread *>
		       (threads()->lookup(Thread_base::myself()));
	} catch (...) {
		return 0;
	}
}


extern "C" void * dde_kit_thread_get_data(struct dde_kit_thread *thread) {
	return static_cast<Dde_kit::Thread_info *>(thread)->data(); }


extern "C" void * dde_kit_thread_get_my_data(void)
{
	try {
		return (threads()->lookup(Thread_base::myself()))->data();
	} catch (...) {
		PERR("current thread not in database");
		return 0;
	}
}


extern "C" void dde_kit_thread_set_data(struct dde_kit_thread *thread, void *data) {
	reinterpret_cast<Dde_kit::Thread_info *>(thread)->data(data); }


extern "C" void dde_kit_thread_set_my_data(void *data)
{
	try {
		(threads()->lookup(Thread_base::myself()))->data(data);
	} catch (...) {
		PERR("current thread not in database");
	}
}


extern "C" void dde_kit_thread_exit(void)
{
	PERR("not implemented yet");

	sleep_forever();
}


extern "C" const char *dde_kit_thread_get_name(struct dde_kit_thread *thread) {
	return reinterpret_cast<Dde_kit::Thread_info *>(thread)->name(); }


extern "C" int dde_kit_thread_get_id(struct dde_kit_thread *thread) {
	return reinterpret_cast<Dde_kit::Thread_info *>(thread)->id(); }


extern "C" void dde_kit_thread_schedule(void)
{
	/* FIXME */
}


/*********************
 ** Sleep interface **
 *********************/

static void _wake_up_msleep(void *lock)
{
	reinterpret_cast<Lock *>(lock)->unlock();
}


extern "C" void dde_kit_thread_msleep(unsigned long msecs)
{
	/*
	 * This simple msleep() registers a timer that fires after 'msecs' and
	 * blocks on 'lock'. The registered timer handler just unlocks 'lock' and
	 * the sleeping thread unblocks.
	 */
	Lock *lock;
	struct dde_kit_timer *timer;

	unsigned long timeout = jiffies + (msecs * DDE_KIT_HZ) / 1000;

	lock = new (env()->heap()) Lock(Lock::LOCKED);
	timer = dde_kit_timer_add(_wake_up_msleep, lock, timeout);

	lock->lock();

	dde_kit_timer_del(timer);
	destroy((env()->heap()), lock);
}


extern "C" void dde_kit_thread_usleep(unsigned long usecs)
{
	unsigned long msecs = usecs / 1000;

	if (msecs > 1)
		dde_kit_thread_msleep(msecs);
	else
		dde_kit_thread_schedule();
}


extern "C" void dde_kit_thread_nsleep(unsigned long nsecs)
{
	unsigned long msecs = nsecs / 1000000;

	if (msecs > 1)
		dde_kit_thread_msleep(msecs);
	else
		dde_kit_thread_schedule();
}
