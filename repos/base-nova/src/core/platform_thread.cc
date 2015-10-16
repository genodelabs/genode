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

/* core includes */
#include <ipc_pager.h>
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


Affinity::Location Platform_thread::affinity() const { return _location; }


int Platform_thread::start(void *ip, void *sp)
{
	using namespace Nova;

	if (!_pager) {
		PERR("pager undefined");
		return -1;
	}

	if (!_pd || (is_main_thread() && !is_vcpu() &&
	             _pd->parent_pt_sel() == Native_thread::INVALID_INDEX)) {
		PERR("protection domain undefined");
		return -2;
	}

	addr_t const pt_oom = _pager->get_oom_portal();
	if (!pt_oom || map_local((Utcb *)Thread_base::myself()->utcb(),
	                         Obj_crd(pt_oom, 0), Obj_crd(_sel_pt_oom(), 0))) {
		PERR("setup of out-of-memory notification portal - failed");
		return -8;
	}

	if (!is_main_thread()) {
		addr_t const initial_sp = reinterpret_cast<addr_t>(sp);
		addr_t const utcb       = is_vcpu() ? 0 : round_page(initial_sp);

		if (_sel_exc_base == Native_thread::INVALID_INDEX) {
			PERR("exception base not specified");
			return -3;
		}

		_pager->assign_pd(_pd->pd_sel());

		/* ip == 0 means that caller will use the thread as worker */
		bool thread_global = ip;

		uint8_t res;
		do {
			res = create_ec(_sel_ec(), _pd->pd_sel(), _location.xpos(),
			                utcb, initial_sp, _sel_exc_base, thread_global);
			if (res == Nova::NOVA_PD_OOM && Nova::NOVA_OK != _pager->handle_oom()) {
				_pd->assign_pd(Native_thread::INVALID_INDEX);
				PERR("creation of new thread failed %u", res);
				return -4;
			}
		} while (res != Nova::NOVA_OK);

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

	addr_t pd_core_sel  = Platform_pd::pd_core_sel();
	addr_t pd_utcb      = 0;
	_sel_exc_base       = is_vcpu() ? _pager->exc_pt_vcpu() : _pager->exc_pt_sel_client();

	if (!is_vcpu()) {
		pd_utcb = Native_config::context_area_virtual_base() +
		          Native_config::context_virtual_size() - get_page_size();

		addr_t remap_src[] = { _pd->parent_pt_sel(), _pager->Object_pool<Pager_object>::Entry::cap().local_name() };
		addr_t remap_dst[] = { PT_SEL_PARENT, PT_SEL_MAIN_PAGER };

		/* remap exception portals for first thread */
		for (unsigned i = 0; i < sizeof(remap_dst)/sizeof(remap_dst[0]); i++) {
			if (map_local((Utcb *)Thread_base::myself()->utcb(),
			              Obj_crd(remap_src[i], 0),
			              Obj_crd(_sel_exc_base + remap_dst[i], 0)))
				return -6;
		}
	}

	/* create task */
	addr_t const pd_sel = cap_map()->insert();
	addr_t const rights = Obj_crd::RIGHT_EC_RECALL |
	                      Obj_crd::RIGHT_PT_CTRL | Obj_crd::RIGHT_PT_CALL |
	                      Obj_crd::RIGHT_SM_UP | Obj_crd::RIGHT_SM_DOWN;
	unsigned pts = is_vcpu() ?  NUM_INITIAL_VCPU_PT_LOG2 : NUM_INITIAL_PT_LOG2;

	enum { KEEP_FREE_PAGES_NOT_AVAILABLE_FOR_UPGRADE = 2, UPPER_LIMIT_PAGES = 32 };
	Obj_crd initial_pts(_sel_exc_base, pts, rights);
	uint8_t res = create_pd(pd_sel, pd_core_sel, initial_pts,
	                        KEEP_FREE_PAGES_NOT_AVAILABLE_FOR_UPGRADE, UPPER_LIMIT_PAGES);
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
	_pager->assign_pd(pd_sel);

	do {
		/* let the thread run */
		res = create_sc(_sel_sc(), pd_sel, _sel_ec(),
		                Qpd(Qpd::DEFAULT_QUANTUM, _priority));
	} while (res == Nova::NOVA_PD_OOM && Nova::NOVA_OK == _pager->handle_oom());

	if (res != NOVA_OK) {
		/*
		 * Reset pd cap since thread got not running and pd cap will
		 * be revoked during cleanup.
		 */
		_pd->assign_pd(Native_thread::INVALID_INDEX);
		_pager->client_set_ec(Native_thread::INVALID_INDEX);
		_pager->initial_eip(0);
		_pager->initial_esp(0);
		_pager->assign_pd(Native_thread::INVALID_INDEX);

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
		uint8_t res;
		do {
			res = create_sc(_sel_sc(), _pd->pd_sel(), _sel_ec(),
			                Qpd(Qpd::DEFAULT_QUANTUM, _priority));
		} while (res == Nova::NOVA_PD_OOM && Nova::NOVA_OK == _pager->handle_oom());

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

	if (is_worker()) {
		s.sp = _pager->initial_esp();
		return s;
	}

	throw Cpu_session::State_access_failed();
}


void Platform_thread::state(Thread_state s)
{
	/* you can do it only once */
	if (_sel_exc_base != Native_thread::INVALID_INDEX)
		throw Cpu_session::State_access_failed();

	/*
	 * s.sel_exc_base exception base of thread in caller
	 *                protection domain - not in Core !
	 * s.is_vcpu      If true it will run as vCPU,
	 *                otherwise it will be a thread.
	 */
	if (!is_main_thread())
		_sel_exc_base = s.sel_exc_base;

	if (!s.is_vcpu)
		return;

	_features |= VCPU;

	if (is_main_thread() && _pager)
		_pager->prepare_vCPU_portals();
}


void Platform_thread::cancel_blocking()
{
	if (!_pager) return;

	_pager->client_cancel_blocking();
}


Native_capability Platform_thread::single_step(bool on)
{
	if (!_pager) return Native_capability();

	Native_capability cap = _pager->single_step(on);

	if (is_worker()) return Native_capability();

	return cap;
}


unsigned long Platform_thread::pager_object_badge() const
{
	return reinterpret_cast<unsigned long>(_name);
}


Weak_ptr<Address_space> Platform_thread::address_space()
{
	return _pd->Address_space::weak_ptr();
}


unsigned long long Platform_thread::execution_time() const
{
	unsigned long long time = 0;

	/*
	 * For local ECs, we simply return 0 as local ECs are executed with the
	 * time of their callers.
	 */
	if (is_worker())
		return time;

	uint8_t res = Nova::sc_ctrl(_sel_sc(), time);
	if (res != Nova::NOVA_OK)
		PDBG("sc_ctrl failed res=%x", res);

	return time;
}


Platform_thread::Platform_thread(const char *name, unsigned prio, int thread_id)
:
	_pd(0), _pager(0), _id_base(cap_map()->insert(2)),
	_sel_exc_base(Native_thread::INVALID_INDEX), _location(boot_cpu(), 0, 0, 0),
	_features(0),
	_priority(Cpu_session::scale_priority(Nova::Qpd::DEFAULT_PRIORITY, prio))
{
	strncpy(_name, name, sizeof(_name));

	if (_priority == 0) {
		PWRN("priority of thread '%s' below minimum - boost to 1", _name);
		_priority = 1;
	}
	if (_priority > Nova::Qpd::DEFAULT_PRIORITY) {
		PWRN("priority of thread '%s' above maximum - limit to %u",
		     _name, Nova::Qpd::DEFAULT_PRIORITY);
		_priority = Nova::Qpd::DEFAULT_PRIORITY;
	}
}


Platform_thread::~Platform_thread()
{
	if (_pager) {
		/* reset pager and badge used for debug output */
		_pager->reset_badge();
		_pager = 0;
	}

	using namespace Nova;

	/* free ec and sc caps */
	revoke(Obj_crd(_id_base, 2));
	cap_map()->remove(_id_base, 2, false);
}
