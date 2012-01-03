/*
 * \brief  Semaphores
 * \author Christian Helmuth
 * \date   2008-09-15
 */

/*
 * Copyright (C) 2008-2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__DDE_KIT__SEMAPHORE_H_
#define _INCLUDE__DDE_KIT__SEMAPHORE_H_

struct dde_kit_sem;
typedef struct dde_kit_sem dde_kit_sem_t;

/**
 * Initialize DDE kit semaphore
 *
 * \param  value  initial semaphore counter
 *
 * \return        pointer to new semaphore
 */
dde_kit_sem_t *dde_kit_sem_init(int value);

/**
 * Deinitialize semaphore
 *
 * \param  sem  semaphore reference
 */
void dde_kit_sem_deinit(dde_kit_sem_t *sem);

/**
 * Acquire semaphore
 *
 * \param  sem  semaphore reference
 */
void dde_kit_sem_down(dde_kit_sem_t *sem);

/**
 * Acquire semaphore (non-blocking)
 *
 * \param  sem  semaphore reference
 *
 * \return      semaphore state
 * \retval 0    semaphore was acquired
 * \retval !=0  semaphore was not acquired
 */
int dde_kit_sem_down_try(dde_kit_sem_t *sem);

/**
 * Acquire semaphore (with timeout)
 *
 * \param  sem      semaphore reference
 * \param  timeout  timeout (in ms)
 *
 * \return          semaphore state
 * \retval 0        semaphore was acquired
 * \retval !=0      semaphore was not acquired
 */
//int dde_kit_sem_down_timed(dde_kit_sem_t *sem, int timout);

/**
 * Release semaphore
 *
 * \param  sem  semaphore reference
 */
void dde_kit_sem_up(dde_kit_sem_t *sem);

#endif /* _INCLUDE__DDE_KIT__SEMAPHORE_H_ */
