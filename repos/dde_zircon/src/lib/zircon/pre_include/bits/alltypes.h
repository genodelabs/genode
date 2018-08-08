/*
 * \brief  Overwrite Zircon specific mutex type
 * \author Johannes Kliemann
 * \date   2018-07-25
 */

/*
 * Copyright (C) 2018 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef __DEFINED_mtx_t
#define __DEFINED_mtx_t

typedef struct
{
	void *lock;
} mtx_t;

#endif /* __DEFINED_mtx_t */

#include_next <bits/alltypes.h>
