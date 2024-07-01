/*
 * \brief  C/C++ interface for this driver
 * \author Sebastian Sumpf
 * \date   2023-07-11
 */

/*
 * Copyright (C) 2023 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

#include <lx_emul/usb_client.h>

#ifdef __cplusplus
extern "C" {
#endif

struct task_struct;

int                 lx_user_main_task(void *);
struct task_struct *lx_user_new_usb_task(int (*func)(void*), void *args,
                                         char const *name);

struct lx_wdm
{
	unsigned long *data_avail;
	void          *buffer;
	void          *handle;
	unsigned       active;
};

int lx_wdm_read(void *args);
int lx_wdm_write(void *args);
int lx_wdm_device(void *args);

void lx_wdm_create_root(void);
void lx_wdm_schedule_read(void *handle);
void lx_wdm_signal_data_avail(void *handle);

#ifdef __cplusplus
} /* extern "C" */
#endif


