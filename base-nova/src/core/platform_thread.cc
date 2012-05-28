/*
 * \brief  Thread facility
 * \author Norman Feske
 * \author Sebastian Sumpf
 * \date   2009-10-02
 */

/*
 * Copyright (C) 2009-2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <base/printf.h>
#include <base/cap_sel_alloc.h>
#include <base/ipc_pager.h>

/* core includes */
#include <platform_thread.h>
#include <platform_pd.h>
#include <util.h>
#include <nova_util.h>

/* NOVA includes */
#include <nova/syscalls.h>

using namespace Genode;


/*********************
 ** Platform thread **
 *********************/

void Platform_thread::set_cpu(unsigned int cpu_no)
{
	PERR("not yet implemented");
}


int Platform_thread::start(void *ip, void *sp, unsigned int cpu_no)
{
	using namespace Nova;

	if (!_pager) {
		PERR("pager undefined");
		return -1;
	}

	enum { PD_EC_CPU_NO = 0, PD_UTCB = 0x6000000 };

	_pager->initial_eip((addr_t)ip);

	if (!_is_main_thread || !_pd) {
		_pager->initial_esp((addr_t)sp);
		return 0;
	}

	/*
	 * For the first thread of a new PD, use the initial stack pointer for
	 * reporting the thread's UTCB address.
	 */
	_pager->initial_esp(PD_UTCB + get_page_size());

	/* locally map parent portal to initial portal window */
	int res = map_local((Nova::Utcb *)Thread_base::myself()->utcb(),
	                    Obj_crd(_pd->parent_pt_sel(), 0),
	                    Obj_crd(_pager->exc_pt_sel() + PT_SEL_PARENT, 0));
	if (res)
		PERR("could not locally remap parent portal");

	Obj_crd initial_pts(_pager->exc_pt_sel(), Nova::NUM_INITIAL_PT_LOG2);


	int pd_sel  = cap_selector_allocator()->pd_sel();
	int pd0_sel = _pager->exc_pt_sel() + Nova::PD_SEL;
	_pd->assign_pd(pd0_sel);

	res = create_pd(pd0_sel, pd_sel, initial_pts);
	if (res)
		PERR("create_pd returned %d", res);

	int ec_sel = cap_selector_allocator()->alloc();
	int sc_sel = cap_selector_allocator()->alloc();

	enum { THREAD_GLOBAL = true };
	res = create_ec(ec_sel, pd0_sel, PD_EC_CPU_NO, PD_UTCB, 0, 0,
	                THREAD_GLOBAL);
	if (res)
		PDBG("create_ec returned %d", res);

	res = create_sc(sc_sel, pd0_sel, ec_sel, Qpd());
	if (res)
		PERR("create_sc returned %d", res);

	return 0;
}


void Platform_thread::pause()
{
	PDBG("not implemented");
}


void Platform_thread::resume()
{
	PDBG("not implemented");
}


int Platform_thread::state(Thread_state *state_dst)
{
	PWRN("not implemented");
	return -1;
}


void Platform_thread::cancel_blocking() { PWRN("not implemented"); }


unsigned long Platform_thread::pager_object_badge()
const
{
	return _pd ? ((_pd->id() << 16) || _id) : ~0;
}


static int id_cnt;


Platform_thread::Platform_thread(const char *name, unsigned, addr_t, int thread_id)
: _pd(0), _id(++id_cnt) { }


Platform_thread::~Platform_thread()
{
	using namespace Nova;

	if (_is_main_thread)
		revoke(Obj_crd(_pd->pd_sel(), 0));
}
