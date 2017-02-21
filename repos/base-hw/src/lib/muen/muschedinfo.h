/*
 * \brief   Muen scheduling group info
 * \author  Adrian-Ken Rueegsegger
 * \date    2016-12-06
 */

/*
 * Copyright (C) 2016 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _BASE__MUEN__MUSCHEDINFO_H_
#define _BASE__MUEN__MUSCHEDINFO_H_

#include <base/stdint.h>

struct scheduling_info_type {
	uint64_t tsc_schedule_start;
	uint64_t tsc_schedule_end;
} __attribute__((packed, aligned (8)));

#endif /* _BASE__MUEN_MUSCHEDINFO_H_ */
