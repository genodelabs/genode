/**
 * \brief  Dummy definitions of Linux Kernel functions
 * \author Christian Helmuth
 * \date   2022-05-02
 */

/*
 * Copyright (C) 2022 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

/* Needed to trace and stop */
#include <lx_emul/debug.h>

#define LX_EMUL_DBG(fmt, ...) \
	printk("\033[32m### %s:%u\033[0m " fmt "\n", __func__, __LINE__, ##__VA_ARGS__)

/* fix for wait_for_completion_timeout where the __sched include is missing */
#include <linux/sched/debug.h>

/* fix for missing include in linux/dynamic_debug.h */
#include <linux/compiler_attributes.h>

/* fix for missing include in acpi/acpi_bus.h */
#include <linux/acpi.h>

/* provide configuration attributes */
#include <i2c_hid_config.h>
