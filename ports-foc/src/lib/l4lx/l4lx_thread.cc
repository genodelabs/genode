/*
 * \brief  L4lxapi library thread functions.
 * \author Stefan Kalkowski
 * \date   2011-04-11
 */

/*
 * Copyright (C) 2011-2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <base/printf.h>
#include <foc_cpu_session/connection.h>

#include <env.h>
#include <vcpu.h>
#include <l4lx_thread.h>

using namespace Fiasco;

enum {
	L4LX_THREAD_NO_THREADS = 48,
	L4LX_THREAD_NAME_LEN   = 32
};

struct l4lx_thread_name_struct {
	l4_cap_idx_t    id;
	char            name[L4LX_THREAD_NAME_LEN];
};

typedef l4_utcb_t *l4lx_thread_t;

static const bool DEBUG = false;
static L4lx::Vcpu* vcpus[L4LX_THREAD_NO_THREADS];
struct l4lx_thread_name_struct l4lx_thread_names[L4LX_THREAD_NO_THREADS];


static l4_addr_t utcb_base_addr()
{
	l4_addr_t _addr = (l4_addr_t) l4_utcb();
	return _addr;
}

Genode::Foc_cpu_connection* L4lx::vcpu_connection()
{
	using namespace Genode;

	static Foc_cpu_connection _con;
	return &_con;
}


static unsigned thread_id(l4lx_thread_t tid)
{
	l4_size_t offset = (l4_addr_t)tid - utcb_base_addr();
	return offset / L4_UTCB_OFFSET;
}


static void* alloc_vcpu_state()
{
	using namespace Fiasco;

	L4lx::Region_manager *rm = L4lx::Env::env()->rm();
	L4lx::Region* r = rm->reserve_range(L4_PAGESIZE, 12);
	if (!r) {
		PWRN("Couldn't allocate vcpu area");
		return 0;
	}
	l4_addr_t addr = r->addr();

	// TODO: use different sizes for 32 vs. 64-bit
	l4_fpage_t fpage = l4_fpage(addr, L4_LOG2_PAGESIZE, L4_CAP_FPAGE_RW);
	l4_msgtag_t tag  = l4_task_add_ku_mem(Fiasco::TASK_CAP, fpage);
	if (l4_error(tag))
		PERR("l4_task_add_ku_mem for %p failed with %ld!",
		     (void*)addr, l4_error(tag));
	return (void*)addr;
}


extern "C" {

void l4lx_thread_name_set(l4_cap_idx_t thread, const char *name)
{
	PWRN("%s: Not implemented yet!", __func__);
}


void l4lx_thread_init(void) { }

void l4lx_thread_alloc_irq(l4_cap_idx_t c)
{
	Genode::Native_capability cap = L4lx::vcpu_connection()->alloc_irq();
	l4_task_map(L4_BASE_TASK_CAP, L4_BASE_TASK_CAP,
	            l4_obj_fpage(cap.dst(), 0, L4_FPAGE_RWX), c | L4_ITEM_MAP);
}

l4lx_thread_t l4lx_thread_create(L4_CV void (*thread_func)(void *data),
                                 unsigned cpu_nr,
                                 void *stack_pointer,
                                 void *stack_data, unsigned stack_data_size,
                                 l4_cap_idx_t l4cap, int prio,
                                 l4_vcpu_state_t **vcpu_state,
                                 const char *name,
                                 struct l4lx_thread_start_info_t *deferstart)
{
	using namespace L4lx;

	if (DEBUG)
		PDBG("func=%p cpu=%x stack=%p data=%p data_size=%x prio=%d name=%s",
		     thread_func, cpu_nr, stack_pointer, stack_data,
		     stack_data_size, prio, name);

	void *addr = 0;
	if (vcpu_state) {
		addr = alloc_vcpu_state();
		if (!addr) {
			PWRN("No kernel-user memory left!");
			return 0;
		}
		*vcpu_state = (l4_vcpu_state_t *) addr;
	}

	Vcpu *vc = new (Genode::env()->heap()) Vcpu(name, thread_func,
	                                            (unsigned long*)stack_data, 1024 * 64,
	                                            (Genode::addr_t)addr,
	                                            l4x_cpu_physmap_get_id(cpu_nr));

	vcpus[thread_id(vc->utcb())] = vc;

	if (!deferstart)
		vc->start();
	else {
		deferstart->l4cap = (l4_cap_idx_t) vc;
		deferstart->sp    = (l4_umword_t)vc->sp();
		deferstart->ip    = (l4_umword_t)vc->ip();
	}

	return vc->utcb();
}


int l4lx_thread_start(struct l4lx_thread_start_info_t *startinfo)
{
	if (DEBUG)
		PDBG("ip=%lx sp=%lx", startinfo->ip, startinfo->sp);
	L4lx::Vcpu *vc = (L4lx::Vcpu*) startinfo->l4cap;
	vc->start();
	return 0;
}


void l4lx_thread_pager_change(l4_cap_idx_t thread, l4_cap_idx_t pager)
{
	if (DEBUG)
		PDBG("Change pager of %lx to %lx", thread, pager);

	l4_cap_idx_t p_id = thread - Fiasco::THREAD_GATE_CAP
	                    + Fiasco::THREAD_PAGER_CAP;

	l4_task_map(L4_BASE_TASK_CAP, L4_BASE_TASK_CAP,
	            l4_obj_fpage(pager, 0, L4_FPAGE_RWX), p_id | L4_ITEM_MAP);
}


void l4lx_thread_set_kernel_pager(l4_cap_idx_t thread)
{
	PWRN("%s: Not implemented yet!", __func__);
}


void l4lx_thread_shutdown(l4lx_thread_t u, void *v)
{
	PWRN("%s: Not implemented yet!", __func__);
}


int l4lx_thread_equal(l4_cap_idx_t t1, l4_cap_idx_t t2)
{
	PWRN("%s: Not implemented yet!", __func__);
	return 0;
}


l4_cap_idx_t l4lx_thread_get_cap(l4lx_thread_t t)
{
	if (!vcpus[thread_id(t)]) {
		PWRN("Invalid utcb %lx", (unsigned long) t);
		return L4_INVALID_CAP;
	}
	return vcpus[thread_id(t)]->tid();
}


int l4lx_thread_is_valid(l4lx_thread_t t)
{
	return vcpus[thread_id(t)] ? 1 : 0;
}

}
