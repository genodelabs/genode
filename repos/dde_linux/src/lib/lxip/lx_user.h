/*
 * \brief  Post kernel activity
 * \author Sebastian Sumpf
 * \date   2024-01-29
 */

/*
 * Copyright (C) 2024 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

#ifdef __cplusplus
extern "C" {
#endif

struct task_struct;

/* set kernel parameters for Genode */
void lx_user_configure_ip_stack(void);

int  lx_user_startup_complete(void *);

void  *lx_socket_dispatch_queue(void);
int    lx_socket_dispatch(void *arg);
struct task_struct *lx_socket_dispatch_root(void);

struct task_struct *lx_user_new_task(int (*func)(void*), void *args);
void                lx_user_destroy_task(struct task_struct *task);

#ifdef __cplusplus
}
#endif
