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

/*******************
 ** linux/mutex.h **
 *******************/

struct mutex
{
	int       state;
	void     *holder;
	void     *waiters;
	unsigned  counter;
	unsigned  id; /* for debugging */
};

#define DEFINE_MUTEX(mutexname) \
	struct mutex mutexname; \
	static void __attribute__((constructor)) mutex_init_ ## mutexname(void) \
	{ mutex_init(&mutexname); }

void mutex_init(struct mutex *m);
void mutex_destroy(struct mutex *m);
void mutex_lock(struct mutex *m);
void mutex_unlock(struct mutex *m);
int mutex_trylock(struct mutex *m);
int mutex_is_locked(struct mutex *m);

/* special case in net/wireless/util.c:1357 */
#define mutex_lock_nested(lock, subclass) mutex_lock(lock)
