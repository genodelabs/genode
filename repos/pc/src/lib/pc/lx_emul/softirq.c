/*
 * \brief  Supplement for emulation of kernel/softirq.c
 * \author Josef Soentgen
 * \date   2022-04-05
 */

/*
 * Copyright (C) 2022 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */


#include <linux/interrupt.h>

void tasklet_setup(struct tasklet_struct * t,
                   void (* callback)(struct tasklet_struct *))
{
	t->next = NULL;
	t->state = 0;
	atomic_set(&t->count, 0);
	t->callback = callback;
	t->use_callback = true;
	t->data = 0;
}


void __tasklet_schedule(struct tasklet_struct * t)
{
	if (test_and_clear_bit(TASKLET_STATE_SCHED, &t->state))
		t->callback(t);
}


void __tasklet_hi_schedule(struct tasklet_struct * t)
{
	if (test_and_clear_bit(TASKLET_STATE_SCHED, &t->state))
		t->callback(t);
}
