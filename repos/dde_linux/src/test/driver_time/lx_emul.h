/**
 * \brief  Dummy definitions of Linux Kernel functions
 * \author Alexander Boettcher
 * \date   2022-07-01
 */

/*
 * Copyright (C) 2022 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

#ifndef _DRIVER_TIME__LX_EMUL_H_
#define _DRIVER_TIME__LX_EMUL_H_

/* Needed to trace and stop */
#include <lx_emul/debug.h>

/* fix for missing includes in generated_dummies */
#include <linux/compiler_attributes.h>
#include <linux/sched/debug.h>
#include <linux/irq.h>

#endif /* _DRIVER_TIME__LX_EMUL_H_ */
