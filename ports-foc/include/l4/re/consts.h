/*
 * \brief  L4Re functions needed by L4Linux
 * \author Stefan Kalkowski
 * \date   2011-03-17
 */

/*
 * Copyright (C) 2011-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _L4__RE__CONSTS_H_
#define _L4__RE__CONSTS_H_

#include <l4/sys/consts.h>

enum {
	L4RE_THIS_TASK_CAP = 1UL << L4_CAP_SHIFT,
};

#endif /* _L4__RE__CONSTS_H_ */
