/*
 * \brief  L4lxapi library thread functions
 * \author Stefan Kalkowski
 * \date   2011-04-29
 */

/*
 * Copyright (C) 2011-2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _L4LX__L4LX_THREAD_H_
#define _L4LX__L4LX_THREAD_H_

#include <linux.h>

namespace Fiasco {
#include <l4/sys/types.h>
#include <l4/sys/utcb.h>
#include <l4/sys/vcpu.h>
}

typedef Fiasco::l4_utcb_t *l4lx_thread_t;

struct l4lx_thread_start_info_t {
	Fiasco::l4_cap_idx_t l4cap;
	Fiasco::l4_umword_t ip, sp;
};

#ifdef __cplusplus
extern "C" {
#endif

FASTCALL Fiasco::l4_cap_idx_t l4x_cpu_thread_get_cap(int cpu);

FASTCALL void l4lx_thread_init(void);
FASTCALL void l4lx_thread_alloc_irq(Fiasco::l4_cap_idx_t c);
FASTCALL l4lx_thread_t l4lx_thread_create(L4_CV void (*thread_func)(void *data),
                                 unsigned cpu_nr,
                                 void *stack_pointer,
                                 void *stack_data, unsigned stack_data_size,
                                 Fiasco::l4_cap_idx_t l4cap, int prio,
                                 Fiasco::l4_vcpu_state_t **vcpu_state,
                                 const char *name,
                                 struct l4lx_thread_start_info_t *deferstart);
FASTCALL int l4lx_thread_start(struct l4lx_thread_start_info_t *startinfo);
FASTCALL int l4lx_thread_is_valid(l4lx_thread_t t);
FASTCALL Fiasco::l4_cap_idx_t l4lx_thread_get_cap(l4lx_thread_t t);
FASTCALL void l4lx_thread_pager_change(Fiasco::l4_cap_idx_t thread,
                                       Fiasco::l4_cap_idx_t pager);
FASTCALL void l4lx_thread_set_kernel_pager(Fiasco::l4_cap_idx_t thread);
FASTCALL void l4lx_thread_shutdown(l4lx_thread_t u, void *v);
FASTCALL int l4lx_thread_equal(Fiasco::l4_cap_idx_t t1, Fiasco::l4_cap_idx_t t2);
FASTCALL void l4lx_thread_utcb_alloc_init(void);

#ifdef __cplusplus
}
#endif

#endif /* _L4LX__L4LX_THREAD_H_ */
