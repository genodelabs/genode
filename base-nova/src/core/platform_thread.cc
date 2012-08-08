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


int Platform_thread::start(void *ip, void *sp, addr_t exc_base)
{
	using namespace Nova;

	if (!_pager) {
		PERR("pager undefined");
		return -1;
	}

	if (!_pd) {
		PERR("protection domain undefined");
		return -2;
	}

	enum { PD_UTCB = 0x6000000 };
	_pager->initial_eip((addr_t)ip);
	if (!_is_main_thread) {
		addr_t initial_sp = reinterpret_cast<addr_t>(sp);
		addr_t utcb       = round_page(initial_sp);

		_pager->initial_esp(initial_sp);
		if (exc_base == ~0UL) {
			PERR("exception base not specified");
			return -3;
		}

		/* ip == 0 means that caller will use the thread as worker */
		bool thread_global = ip;
		uint8_t res = create_ec(_sel_ec(), _pd->pd_sel(), _cpu_no,
					utcb, initial_sp,
					exc_base, thread_global);
		if (res)
			PERR("creation of new thread failed %u", res);

		return res ? -4 : 0;
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
	if (res) {
		PERR("could not locally remap parent portal");
		return -5;
	}

	Obj_crd initial_pts(_pager->exc_pt_sel(), Nova::NUM_INITIAL_PT_LOG2,
	                    1 << 4);


	addr_t pd_sel  = cap_selector_allocator()->pd_sel();
	addr_t pd0_sel = _pager->exc_pt_sel() + Nova::PD_SEL;
	_pd->assign_pd(pd0_sel);

	res = create_pd(pd0_sel, pd_sel, initial_pts);
	if (res) {
		PERR("create_pd returned %d", res);
		return -6;
	}

	enum { THREAD_GLOBAL = true };
	res = create_ec(_sel_ec(), pd0_sel, _cpu_no, PD_UTCB, 0, 0,
	                THREAD_GLOBAL);
	if (res) {
		PERR("create_ec returned %d", res);
		return -7;
	}

	res = create_sc(_sel_sc(), pd0_sel, _sel_ec(), Qpd());
	if (res) {
		PERR("create_sc returned %d", res);
		return -8;
	}

	return 0;
}


void Platform_thread::pause()
{
	PDBG("not implemented");
}


void Platform_thread::resume()
{
	uint8_t res = Nova::create_sc(_sel_sc(), _pd->pd_sel(), _sel_ec(),
	                              Nova::Qpd());
	if (res)
		PDBG("create_sc returned %u", res);
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
	return ~0UL;
}


Platform_thread::Platform_thread(const char *name, unsigned, int thread_id)
: _pd(0), _id_base(cap_selector_allocator()->alloc(1)),
  _cpu_no(0) { }


Platform_thread::~Platform_thread()
{
	using namespace Nova;

	if (_is_main_thread)
		revoke(Obj_crd(_pd->pd_sel(), 0));
}
