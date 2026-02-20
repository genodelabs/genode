/*
 * \brief  USB-serial for Modems
 * \author Sebastian Sumpf
 * \date   2026-02-20
 */

/*
 * Copyright (C) 2026 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2 or later.
 */

#ifndef _USB_OPTION_H_
#define _USB_OPTION_H_

#ifdef __cplusplus
extern "C" {
#endif
	void lx_option_init(void);
	void lx_option_handle_io(void);
#ifdef __cplusplus
} /* extern "C" */
#endif

#ifndef __cplusplus
#include <linux/cdev.h>
#include <linux/tty.h>

struct lx_option
{
	struct   cdev         cdev;
	struct   inode        inode;
	struct   file         file;
	struct   task_struct *task;
	dev_t                 dev;
	bool                  run;
	struct   list_head    list;
};

void lx_option_create_session(struct lx_option *);
void lx_option_destroy_session(struct lx_option *);
#endif

#endif /* _USB_OPTION_H_ */
