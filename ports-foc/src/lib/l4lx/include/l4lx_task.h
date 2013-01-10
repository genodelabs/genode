/*
 * \brief  L4lxapi library task functions
 * \author Stefan Kalkowski
 * \date   2011-04-29
 */

/*
 * Copyright (C) 2011-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _L4LX__L4LX_TASK_H_
#define _L4LX__L4LX_TASK_H_

#include <linux.h>

namespace Fiasco {
#include <l4/sys/types.h>
}

#ifdef __cplusplus
extern "C" {
#endif

FASTCALL void l4lx_task_init(void);
FASTCALL Fiasco::l4_cap_idx_t l4lx_task_number_allocate(void);
FASTCALL int l4lx_task_number_free(Fiasco::l4_cap_idx_t task);
FASTCALL int l4lx_task_get_new_task(Fiasco::l4_cap_idx_t parent_id,
                                    Fiasco::l4_cap_idx_t *id);
FASTCALL int l4lx_task_create(Fiasco::l4_cap_idx_t task_no);
FASTCALL int l4lx_task_create_thread_in_task(Fiasco::l4_cap_idx_t thread,
                                             Fiasco::l4_cap_idx_t task,
                                             Fiasco::l4_cap_idx_t pager,
                                             unsigned cpu);
FASTCALL int l4lx_task_create_pager(Fiasco::l4_cap_idx_t task_no,
                                    Fiasco::l4_cap_idx_t pager);
FASTCALL int l4lx_task_delete_thread(Fiasco::l4_cap_idx_t thread);
FASTCALL int l4lx_task_delete_task(Fiasco::l4_cap_idx_t task, unsigned option);

#ifdef __cplusplus
}
#endif

#endif /* _L4LX__L4LX_TASK_H_ */
