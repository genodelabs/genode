/*
 * \brief  Replaces kernel/irq/spurious.c
 * \author Stefan Kalkowski
 * \date   2021-03-16
 */

/*
 * Copyright (C) 2021 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

#include <linux/types.h>

extern bool noirqdebug;
bool noirqdebug = 1;
