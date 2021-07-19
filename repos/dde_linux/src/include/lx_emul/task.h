/*
 * \brief  Lx_emul support to task handling
 * \author Stefan Kalkowski
 * \date   2021-03-25
 */

/*
 * Copyright (C) 2021 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

#ifndef _LX_EMUL__TASK_H_
#define _LX_EMUL__TASK_H_

#ifdef __cplusplus
extern "C" {
#endif

enum { SWAPPER_PID, KIRQ_PID, FIRST_PID };

struct task_struct;

struct task_struct * lx_emul_task_get_current(void);

struct task_struct * lx_emul_task_get(int pid);

void lx_emul_task_create(struct task_struct * task,
                         const char * name,
                         int pid,
                         int (* threadfn)(void * data),
                         void * data);

void lx_emul_task_unblock(struct task_struct * task);

void lx_emul_task_priority(struct task_struct * task, unsigned long prio);

void lx_emul_task_schedule(int block);

void lx_emul_task_name(struct task_struct * task, const char * name);

#ifdef __cplusplus
}
#endif

#endif /* _LX_EMUL__TASK_H_ */
