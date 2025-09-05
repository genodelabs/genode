/*
 * \brief  Emulation of Linux Kernel functions
 * \author Christian Helmuth
 * \date   2025-09-02
 */

/*
 * Copyright (C) 2025 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2 or later.
 */

#ifndef _LX_EMUL_H_
#define _LX_EMUL_H_

#include <lx_emul/debug.h>
#include <linux/init.h>
#include <linux/sched/debug.h>
#include <linux/usb.h>

#include <genode_c_api/terminal.h>
#include <genode_c_api/usb_client.h>

struct usb_hub;

void lx_emul_usb_serial_wait_for_device(void);

void lx_emul_usb_serial_write(struct genode_const_buffer);

unsigned long lx_emul_usb_serial_read(struct genode_buffer);

#endif /* _LX_EMUL_H_ */
