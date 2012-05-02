/*
 * \brief  Timers and tick
 * \author Christian Helmuth
 * \date   2008-10-22
 *
 * DDE kit provides a generic timer implementation that enables users to
 * execute a callback function after a certain period of time. Therefore, DDE
 * kit starts a timer thread that executes callbacks and keeps track of the
 * currently running timers.
 */

/*
 * Copyright (C) 2008-2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__DDE_KIT__TIMER_H_
#define _INCLUDE__DDE_KIT__TIMER_H_

#include <dde_kit/thread.h>


/**********
 ** Tick **
 **********/

/** Timer tick (global symbol) */
extern volatile unsigned long dde_kit_timer_ticks;
extern volatile unsigned long jiffies;

/** Timer tick rate */
enum { DDE_KIT_HZ = 100 };


/***********
 ** Timer **
 ***********/

struct dde_kit_timer;

/**
 * Add timer event handler
 *
 * \param fn       function to call on timeout
 * \param priv     private handler token
 * \param timeout  absolute timeout (in DDE kit ticks)
 *
 * \return timer reference on success; 0 otherwise 
 *
 * After the absolute timeout has expired, function fn is called with args as
 * arguments.
 */
struct dde_kit_timer *dde_kit_timer_add(void (*fn)(void *), void *priv,
                                        unsigned long timeout);

/**
 * Delete timer
 *
 * \param timer  timer reference
 */
void dde_kit_timer_del(struct dde_kit_timer *timer);


/**
 * Schedule absolute timeout
 *
 * \param timer    timer reference
 * \param timeout  absolute timeout (in DDE kit ticks)
 */
void dde_kit_timer_schedule_absolute(struct dde_kit_timer *timer, unsigned long timeout);


/**
 * Check whether a timer is pending
 *
 * \param timer  timer reference
 */
int dde_kit_timer_pending(struct dde_kit_timer *timer);

/** Init timers and ticks */
void dde_kit_timer_init(void(*thread_init)(void *), void *priv);

#endif /* _INCLUDE__DDE_KIT__TIMER_H_ */
