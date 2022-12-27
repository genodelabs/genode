/**
 * \brief  Dummy definitions of Linux Kernel functions
 * \author Stefan Kalkowski
 * \date   2022-01-07
 */

/*
 * Copyright (C) 2021 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

/* Needed to trace and stop */
#include <lx_emul/debug.h>

/* fix for missing includes in generated_dummies */
#include <linux/compiler_attributes.h>
#include <linux/sched/debug.h>
#include <net/rtnetlink.h>


struct rtnl_link_ops *wireguard_rtnl_link_ops(void);
