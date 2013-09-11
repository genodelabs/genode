/*
 * \brief  Thread facility
 * \author Norman Feske
 * \author Sebastian Sumpf
 * \author Alexander Boettcher
 * \date   2009-10-02
 */

/*
 * Copyright (C) 2009-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <base/printf.h>
#include <base/ipc_pager.h>

/* core includes */
#include <platform_thread.h>
#include <platform_pd.h>
#include <util.h>
#include <nova_util.h>

/* NOVA includes */
#include <nova/syscalls.h>
#include <nova/util.h>

using namespace Genode;


/*********************
 ** Platform thread **
 *********************/

void Platform_thread::affinity(Affinity::Location location)
{
	if (_sel_exc_base != Native_thread::INVALID_INDEX) {
		PERR("Failure - affinity of thread could not be set");
		return;
	}

	_location = location;
}


Affinity::Location Platform_thread::affinity() { return _location; }


int Platform_thread::start(void *ip, void *sp)
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

	if (!is_main_thread()) {
		addr_t initial_sp = reinterpret_cast<addr_t>(sp);
		addr_t utcb       = is_vcpu() ? 0 : round_page(initial_sp);

		if (_sel_exc_base == Native_thread::INVALID_INDEX) {
			PERR("exception base not specified");
			return -3;
		}

		/* ip == 0 means that caller will use the thread as worker */
		bool thread_global = ip;

		uint8_t res = create_ec(_sel_ec(), _pd->pd_sel(), _location.xpos(),
		                        utcb, initial_sp, _sel_exc_base, thread_global);
		if (res != Nova::NOVA_OK) {
			PERR("creation of new thread failed %u", res);
			return -4;
		}

		if (!thread_global) {
			_features |= WORKER;

			/* local/worker threads do not require a startup portal */
			revoke(Obj_crd(_pager->exc_pt_sel_client() + PT_SEL_STARTUP, 0));
		}

		_pager->initial_eip((addr_t)ip);
		_pager->initial_esp(initial_sp);
		_pager->client_set_ec(_sel_ec());

		return 0;
	}

	if (_sel_exc_base != Native_thread::INVALID_INDEX) {
		PERR("thread already started");
		return -5;
	}

	/*
	 * For the first thread of a new PD, use the initial stack pointer for
	 * reporting the thread's UTCB address.
	 */
	addr_t pd_utcb = Native_config::context_area_virtual_base() +
	                 Native_config::context_area_virtual_size() -
	                 get_page_size();

	_sel_exc_base = _pager->exc_pt_sel_client();

	addr_t pd_core_sel  = Platform_pd::pd_core_sel();

	addr_t remap_src[] = { _pd->parent_pt_sel() };
	addr_t remap_dst[] = { PT_SEL_PARENT };
	addr_t pd_sel;

	Obj_crd initial_pts(_sel_exc_base, NUM_INITIAL_PT_LOG2);

	uint8_t res;

	/* remap exception portals for first thread */
	for (unsigned i = 0; i < sizeof(remap_dst)/sizeof(remap_dst[0]); i++) {
		if (map_local((Utcb *)Thread_base::myself()->utcb(),
		              Obj_crd(remap_src[i], 0),
		              Obj_crd(_sel_exc_base + remap_dst[i], 0)))
			return -6;
	}

	pd_sel = cap_map()->insert();

	/* create task */
	res = create_pd(pd_sel, pd_core_sel, initial_pts);
	if (res != NOVA_OK) {
		PERR("create_pd returned %d", res);
		goto cleanup_pd;
	}

	/* create first thread in task */
	enum { THREAD_GLOBAL = true };
	res = create_ec(_sel_ec(), pd_sel, _location.xpos(), pd_utcb, 0, 0,
	                THREAD_GLOBAL);
	if (res != NOVA_OK) {
		PERR("create_ec returned %d", res);
		goto cleanup_pd;
	}

	/*
	 * We have to assign the pd here, because after create_sc the thread
	 * becomes running immediately.
	 */
	_pd->assign_pd(pd_sel);
	_pager->client_set_ec(_sel_ec());
	_pager->initial_eip((addr_t)ip);
	_pager->initial_esp((addr_t)sp);

	/* let the thread run */
	res = create_sc(_sel_sc(), pd_sel, _sel_ec(), Qpd());
	if (res != NOVA_OK) {
		/*
		 * Reset pd cap since thread got not running and pd cap will
		 * be revoked during cleanup.
		 */
		_pd->assign_pd(Native_thread::INVALID_INDEX);
		_pager->client_set_ec(Native_thread::INVALID_INDEX);
		_pager->initial_eip(0);
		_pager->initial_esp(0);

		PERR("create_sc returned %d", res);
		goto cleanup_ec;
	}

	return 0;

	cleanup_ec:
	/* cap_selector free for _sel_ec is done in de-constructor */
	revoke(Obj_crd(_sel_ec(), 0));

	cleanup_pd:
	revoke(Obj_crd(pd_sel, 0));
	cap_map()->remove(pd_sel, 0, false);

	return -7;
}


Native_capability Platform_thread::pause()
{
	if (!_pager) return Native_capability();

	Native_capability notify_sm = _pager->notify_sm();
	if (!notify_sm.valid()) return notify_sm;

	if (_pager->client_recall() != Nova::NOVA_OK)
		return Native_capability();

	/* If the thread is blocked in its own SM, get him out */
	cancel_blocking();

	/* local thread may never get be canceled if it doesn't receive an IPC */
	if (is_worker()) return Native_capability();

	return notify_sm;
}


void Platform_thread::resume()
{
	using namespace Nova;

	if (!is_worker()) {
		uint8_t res = create_sc(_sel_sc(), _pd->pd_sel(), _sel_ec(), Qpd());
		if (res == NOVA_OK) return;
	}

	if (!_pager) return;

	/* Thread was paused beforehand and blocked in pager - wake up pager */
	_pager->wake_up();
}


Thread_state Platform_thread::state()
{
	if (!_pager) throw Cpu_session::State_access_failed();

	Thread_state s;
	if (_pager->copy_thread_state(&s))
		return s;

	if (is_worker())
		s.sp = _pager->initial_esp();

	return s;
}


void Platform_thread::state(Thread_state s)
{
	/* not permitted for main thread */
	if (is_main_thread()) throw Cpu_session::State_access_failed();

	/* you can do it only once */
	if (_sel_exc_base != Native_thread::INVALID_INDEX)
		throw Cpu_session::State_access_failed();

	/*
	 * s.sel_exc_base exception base of thread in caller
	 *                protection domain - not in Core !
	 * s.is_vcpu      If true it will run as vCPU,
	 *                otherwise it will be a thread.
	 */
	_sel_exc_base = s.sel_exc_base;
	if (s.is_vcpu) _features |= VCPU;
}


void Platform_thread::cancel_blocking()
{
	if (!_pager) return;

	_pager->client_cancel_blocking();
}


void Platform_thread::single_step(bool on)
{
	if (!_pager) return;

	_pager->single_step(on);
}


unsigned long Platform_thread::pager_object_badge() const
{
	return reinterpret_cast<unsigned long>(_name);
}


Weak_ptr<Address_space> Platform_thread::address_space()
{
	return _pd->Address_space::weak_ptr();
}


Platform_thread::Platform_thread(const char *name, unsigned, int thread_id)
:
	_pd(0), _pager(0), _id_base(cap_map()->insert(1)),
	_sel_exc_base(Native_thread::INVALID_INDEX), _location(boot_cpu(), 0),
	_features(0)
{
	strncpy(_name, name, sizeof(_name));
}


Platform_thread::~Platform_thread()
{
	using namespace Nova;

	/* free ec and sc caps */
	revoke(Obj_crd(_id_base, 1));
	cap_map()->remove(_id_base, 1, false);
}
