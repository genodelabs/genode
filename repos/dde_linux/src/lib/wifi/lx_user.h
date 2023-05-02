/**
 * \brief  Lx user definitions
 * \author Josef Soentgen
 * \date   2022-05-12
 */

/*
 * Copyright (C) 2022 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

#ifdef __cplusplus
extern "C" {
#endif

struct task_struct;

extern struct task_struct *uplink_task_struct_ptr;
void uplink_init(void);

extern struct task_struct *socketcall_task_struct_ptr;
void socketcall_init(void);

extern struct task_struct *rfkill_task_struct_ptr;
void rfkill_init(void);

#ifdef __cplusplus
}
#endif
