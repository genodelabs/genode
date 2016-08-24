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
#include <base/log.h>

/* core includes */
#include <ipc_pager.h>
#include <platform_thread.h>
#include <platform_pd.h>
#include <util.h>
#include <nova_util.h>

/* base-internal includes */
#include <base/internal/stack_area.h>

/* NOVA includes */
#include <nova/syscalls.h>
#include <nova/util.h>

using namespace Genode;


/*********************
 ** Platform thread **
 *********************/

void Platform_thread::affinity(Affinity::Location location)
{
	error("dynamic affinity change not supported on NOVA");
}


Affinity::Location Platform_thread::affinity() const { return _location; }


int Platform_thread::start(void *ip, void *sp)
{
	using namespace Nova;

	if (!_pager) {
		error("pager undefined");
		return -1;
	}

	if (!_pd || (main_thread() && !vcpu() &&
	             _pd->parent_pt_sel() == Native_thread::INVALID_INDEX)) {
		error("protection domain undefined");
		return -2;
	}

	addr_t const pt_oom = _pager->get_oom_portal();
	if (!pt_oom || map_local((Utcb *)Thread::myself()->utcb(),
	                         Obj_crd(pt_oom, 0), Obj_crd(_sel_pt_oom(), 0))) {
		error("setup of out-of-memory notification portal - failed");
		return -8;
	}

	if (!main_thread()) {
		addr_t const initial_sp = reinterpret_cast<addr_t>(sp);
		addr_t const utcb       = vcpu() ? 0 : round_page(initial_sp);

		if (_sel_exc_base == Native_thread::INVALID_INDEX) {
			error("exception base not specified");
			return -3;
		}

		_pager->assign_pd(_pd->pd_sel());

		uint8_t res;
		do {
			res = create_ec(_sel_ec(), _pd->pd_sel(), _location.xpos(),
			                utcb, initial_sp, _sel_exc_base, !worker());
			if (res == Nova::NOVA_PD_OOM && Nova::NOVA_OK != _pager->handle_oom()) {
				_pager->assign_pd(Native_thread::INVALID_INDEX);
				error("creation of new thread failed ", res);
				return -4;
			}
		} while (res != Nova::NOVA_OK);

		if (worker()) {
			/* local/worker threads do not require a startup portal */
			revoke(Obj_crd(_pager->exc_pt_sel_client() + PT_SEL_STARTUP, 0));
		}

		_pager->initial_eip((addr_t)ip);
		_pager->initial_esp(initial_sp);
		_pager->client_set_ec(_sel_ec());

		return 0;
	}

	if (_sel_exc_base != Native_thread::INVALID_INDEX) {
		error("thread already started");
		return -5;
	}

	addr_t pd_core_sel  = Platform_pd::pd_core_sel();
	addr_t pd_utcb      = 0;
	_sel_exc_base       = vcpu() ? _pager->exc_pt_vcpu() : _pager->exc_pt_sel_client();

	if (!vcpu()) {
		pd_utcb = stack_area_virtual_base() + stack_virtual_size() - get_page_size();

		addr_t remap_src[] = { _pd->parent_pt_sel(),
		                       (unsigned long)_pager->Object_pool<Pager_object>::Entry::cap().local_name() };
		addr_t remap_dst[] = { PT_SEL_PARENT, PT_SEL_MAIN_PAGER };

		/* remap exception portals for first thread */
		for (unsigned i = 0; i < sizeof(remap_dst)/sizeof(remap_dst[0]); i++) {
			if (map_local((Utcb *)Thread::myself()->utcb(),
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
	unsigned pts = vcpu() ?  NUM_INITIAL_VCPU_PT_LOG2 : NUM_INITIAL_PT_LOG2;

	enum { KEEP_FREE_PAGES_NOT_AVAILABLE_FOR_UPGRADE = 2, UPPER_LIMIT_PAGES = 32 };
	Obj_crd initial_pts(_sel_exc_base, pts, rights);
	uint8_t res = create_pd(pd_sel, pd_core_sel, initial_pts,
	                        KEEP_FREE_PAGES_NOT_AVAILABLE_FOR_UPGRADE, UPPER_LIMIT_PAGES);
	if (res != NOVA_OK) {
		error("create_pd returned ", res);
		goto cleanup_pd;
	}

	/* create first thread in task */
	enum { THREAD_GLOBAL = true };
	res = create_ec(_sel_ec(), pd_sel, _location.xpos(), pd_utcb, 0, 0,
	                THREAD_GLOBAL);
	if (res != NOVA_OK) {
		error("create_ec returned ", res);
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

		error("create_sc returned ", res);
		goto cleanup_ec;
	} else
		_features |= SC_CREATED;

	return 0;

	cleanup_ec:
	/* cap_selector free for _sel_ec is done in de-constructor */
	revoke(Obj_crd(_sel_ec(), 0));

	cleanup_pd:
	revoke(Obj_crd(pd_sel, 0));
	cap_map()->remove(pd_sel, 0, false);

	return -7;
}


void Platform_thread::pause()
{
	if (!_pager)
		return;

	_pager->client_recall(true);
}


void Platform_thread::resume()
{
	using namespace Nova;

	if (worker() || sc_created()) {
		if (_pager)
			_pager->wake_up();
		return;
	}

	uint8_t res;
	do {
		if (!_pd) {
			error("protection domain undefined - resuming thread failed");
			return;
		}
		res = create_sc(_sel_sc(), _pd->pd_sel(), _sel_ec(),
		                Qpd(Qpd::DEFAULT_QUANTUM, _priority));
	} while (res == Nova::NOVA_PD_OOM && Nova::NOVA_OK == _pager->handle_oom());

	if (res == NOVA_OK)
		_features |= SC_CREATED;
	else
		error("create_sc failed ", res);
}


Thread_state Platform_thread::state()
{
	if (!_pager) throw Cpu_thread::State_access_failed();

	Thread_state s;

	if (_pager->copy_thread_state(&s))
		return s;

	throw Cpu_thread::State_access_failed();
}


void Platform_thread::state(Thread_state s)
{
	if (_sel_exc_base == Native_thread::INVALID_INDEX) {

		/* you can do it only once */

		/*
		 * s.sel_exc_base exception base of thread in caller
		 *                protection domain - not in Core !
		 * s.is_vcpu      If true it will run as vCPU,
		 *                otherwise it will be a thread.
		 */
		if (!main_thread())
			_sel_exc_base = s.sel_exc_base;

		if (!s.global_thread)
			_features |= WORKER;

		if (!s.vcpu)
			return;

		_features |= VCPU;

		if (main_thread() && _pager)
			_pager->prepare_vCPU_portals();

	} else {

		if (!_pager) throw Cpu_thread::State_access_failed();

		if (!_pager->copy_thread_state(s))
			throw Cpu_thread::State_access_failed();

		/* the new state is transferred to the kernel by the recall handler */
		_pager->client_recall(false);
	}
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

const char * Platform_thread::pd_name() const {
	return _pd ? _pd->name() : "unknown"; }

Weak_ptr<Address_space> Platform_thread::address_space()
{
	if (!_pd) {
		error(__PRETTY_FUNCTION__, ": protection domain undefined");
		return Weak_ptr<Address_space>();
	}
	return _pd->Address_space::weak_ptr();
}


unsigned long long Platform_thread::execution_time() const
{
	unsigned long long time = 0;

	/* for ECs without a SC we simply return 0 */
	if (!sc_created())
		return time;

	uint8_t res = Nova::sc_ctrl(_sel_sc(), time);
	if (res != Nova::NOVA_OK)
		warning("sc_ctrl failed res=", res);

	return time;
}


Platform_thread::Platform_thread(size_t, const char *name, unsigned prio,
                                 Affinity::Location affinity, int thread_id)
:
	_pd(0), _pager(0), _id_base(cap_map()->insert(2)),
	_sel_exc_base(Native_thread::INVALID_INDEX), _location(affinity),
	_features(0),
	_priority(Cpu_session::scale_priority(Nova::Qpd::DEFAULT_PRIORITY, prio)),
	_name(name)
{
	if (_priority == 0) {
		warning("priority of thread '", _name, "' below minimum - boost to 1");
		_priority = 1;
	}
	if (_priority > Nova::Qpd::DEFAULT_PRIORITY) {
		warning("priority of thread '", _name, "' above maximum - limit to ",
		        (unsigned)Nova::Qpd::DEFAULT_PRIORITY);
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
