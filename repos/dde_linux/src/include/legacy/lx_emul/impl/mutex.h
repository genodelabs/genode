/*
 * \brief  Implementation of linux/mutex.h
 * \author Norman Feske
 * \date   2015-09-09
 */

/*
 * Copyright (C) 2015-2017 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

/* Linux kit includes */
#include <legacy/lx_kit/scheduler.h>
#include <legacy/lx_kit/env.h>

enum { MUTEX_UNLOCKED = 1, MUTEX_LOCKED = 0, MUTEX_WAITERS = -1 };

void mutex_init(struct mutex *m)
{
	static unsigned id = 0;

	m->state   = MUTEX_UNLOCKED;
	m->holder  = nullptr;
	m->waiters = nullptr;
	m->id      = ++id;
	m->counter = 0;
}


void mutex_destroy(struct mutex *m)
{
	Lx::Task::List *waiters = static_cast<Lx::Task::List *>(m->waiters);

	/* FIXME potentially blocked tasks are not unblocked */
	if (waiters && waiters->first()) {
		Genode::error(__func__, "destroying non-empty waiters list");
	}

	Genode::destroy(&Lx_kit::env().heap(), waiters);

	m->holder  = nullptr;
	m->waiters = nullptr;
	m->id      = 0;
	m->counter = 0;
}


static inline void __check_or_initialize_mutex(struct mutex *m)
{
	if (!m->waiters) {
		m->waiters = new (&Lx_kit::env().heap()) Lx::Task::List;
	}
}


void mutex_lock(struct mutex *m)
{
	__check_or_initialize_mutex(m);

	while (1) {
		if (m->state == MUTEX_UNLOCKED) {
			m->state  = MUTEX_LOCKED;
			m->holder = Lx::scheduler().current();

			break;
		}

		Lx::Task *t = reinterpret_cast<Lx::Task *>(m->holder);

		if (t == Lx::scheduler().current()) {
			m->counter++;
			return;
		}

		/* notice that a task waits for the mutex to be released */
		m->state = MUTEX_WAITERS;

		/* block until the mutex is released (and retry then) */
		Lx::scheduler().current()->mutex_block(static_cast<Lx::Task::List *>(m->waiters));
		Lx::scheduler().current()->schedule();
	}
}


void mutex_unlock(struct mutex *m)
{
	if (m->state == MUTEX_UNLOCKED) {
		Genode::error("bug: multiple mutex unlock detected");
		Genode::sleep_forever();
	}
	if (m->holder != Lx::scheduler().current()) {
		Genode::error("bug: mutex unlock by task not holding the mutex");
		Genode::sleep_forever();
	}

	if (m->counter) {
		m->counter--;
		return;
	}

	Lx::Task::List *waiters = static_cast<Lx::Task::List *>(m->waiters);

	if (m->state == MUTEX_WAITERS)
		while (Lx::Task::List_element *le = waiters->first())
			le->object()->mutex_unblock(waiters);

	m->state  = MUTEX_UNLOCKED;
	m->holder = nullptr;
}


int mutex_is_locked(struct mutex *m)
{
	return m->state != MUTEX_UNLOCKED;
}


int mutex_trylock(struct mutex *m)
{
	if (mutex_is_locked(m))
		return false;

	mutex_lock(m);
	return true;
}
