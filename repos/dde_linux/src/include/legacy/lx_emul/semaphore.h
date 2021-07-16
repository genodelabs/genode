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
 ** linux/rwsem.h **
 *******************/

struct rw_semaphore { int dummy; };

#define DECLARE_RWSEM(name) \
	struct rw_semaphore name = { 0 }

#define init_rwsem(sem) do { (void)sem; } while (0)

void down_read(struct rw_semaphore *sem);
void up_read(struct rw_semaphore *sem);
void down_write(struct rw_semaphore *sem);
void up_write(struct rw_semaphore *sem);
int  down_write_killable(struct rw_semaphore *);

#define __RWSEM_INITIALIZER(name) { 0 }
