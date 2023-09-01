/*
 * \brief  This file shadows linux/init.h
 * \author Sebastian Sumpf
 * \date   2024-01-29
 */

/*
 * Copyright (C) 2024 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

#ifndef _SHADOW__LINUX__INIT_H_
#define _SHADOW__LINUX__INIT_H_

/* include the next linux/init.h found in include paths */
#include_next <linux/init.h>

#undef __setup

/*
 * '__setup' calls are currently only used by lxip, keep ti local in order to
 * avoid problems with other projects
 */

#define __setup(str, fn) \
	int __setup_##fn(char * string) { return fn(string); }

#endif
