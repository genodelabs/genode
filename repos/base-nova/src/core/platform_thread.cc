/*
 * \brief  Thread facility
 * \author Norman Feske
 * \author Sebastian Sumpf
 * \author Alexander Boettcher
 * \date   2009-10-02
 */

/*
 * Copyright (C) 2009-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <base/log.h>

/* core includes */
#include <ipc_pager.h>
#include <platform.h>
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


static uint8_t map_thread_portals(Pager_object &pager,
                                  addr_t const target_exc_base,
                                  Nova::Utcb &utcb)
{
	using Nova::Obj_crd;
	using Nova::NUM_INITIAL_PT_LOG2;

	addr_t const source_pd = platform_specific().core_pd_sel();
	addr_t const source_exc_base = pager.exc_pt_sel_client();
	addr_t const target_pd = pager.pd_sel();

	/* xxx better map portals with solely pt_call and sm separately ? xxx */
	addr_t const rights = Obj_crd::RIGHT_EC_RECALL |
	                      Obj_crd::RIGHT_PT_CTRL | Obj_crd::RIGHT_PT_CALL | Obj_crd::RIGHT_PT_XCPU |
	                      Obj_crd::RIGHT_SM_UP | Obj_crd::RIGHT_SM_DOWN;

	Obj_crd const source_initial_caps(source_exc_base, NUM_INITIAL_PT_LOG2,
		                              rights);
	Obj_crd const target_initial_caps(target_exc_base, NUM_INITIAL_PT_LOG2,
	                                  rights);

	return async_map(pager, source_pd, target_pd,
	                 source_initial_caps, target_initial_caps, utcb);
}


/*********************
 ** Platform thread **
 *********************/


void Platform_thread::affinity(Affinity::Location location)
{
	if (!_pager)
		return;

	if (worker() || vcpu() || !sc_created())
		return;

	_pager->migrate(platform_specific().sanitize(location));
}


bool Platform_thread::_create_and_map_oom_portal(Nova::Utcb &utcb)
{
	addr_t const pt_oom = pager().create_oom_portal();
	if (!pt_oom)
		return false;

	addr_t const source_pd = platform_specific().core_pd_sel();
	return !map_local(source_pd, utcb, Nova::Obj_crd(pt_oom, 0),
	                  Nova::Obj_crd(_sel_pt_oom(), 0));
}


void Platform_thread::prepare_migration()
{
	using Nova::Utcb;
	Utcb &utcb = *reinterpret_cast<Utcb *>(Thread::myself()->utcb());

	/* map exception portals to target pd */
	map_thread_portals(pager(), _sel_exc_base, utcb);
	/* re-create pt_oom portal */
	_create_and_map_oom_portal(utcb);
}


int Platform_thread::start(void *ip, void *sp)
{
	using namespace Nova;

	if (!_pager) {
		error("pager undefined");
		return -1;
	}

	Pager_object &pager = *_pager;

	if (!_pd || (main_thread() && !vcpu() &&
	             _pd->parent_pt_sel() == Native_thread::INVALID_INDEX)) {
		error("protection domain undefined");
		return -2;
	}

	Utcb &utcb = *reinterpret_cast<Utcb *>(Thread::myself()->utcb());
	unsigned const kernel_cpu_id = platform_specific().kernel_cpu_id(_location);
	addr_t const source_pd = platform_specific().core_pd_sel();

	if (!_create_and_map_oom_portal(utcb)) {
		error("setup of out-of-memory notification portal - failed");
		return -8;
	}

	if (!main_thread()) {
		addr_t const initial_sp = reinterpret_cast<addr_t>(sp);
		addr_t const utcb_addr  = vcpu() ? 0 : round_page(initial_sp);

		if (_sel_exc_base == Native_thread::INVALID_INDEX) {
			error("exception base not specified");
			return -3;
		}

		uint8_t res = syscall_retry(pager,
			[&]() {
				return create_ec(_sel_ec(), _pd->pd_sel(), kernel_cpu_id,
				                 utcb_addr, initial_sp, _sel_exc_base,
				                 !worker());
			});

		if (res != Nova::NOVA_OK) {
			error("creation of new thread failed ", res);
			return -4;
		}

		if (!vcpu())
			res = map_thread_portals(pager, _sel_exc_base, utcb);

		if (res != NOVA_OK) {
			revoke(Obj_crd(_sel_ec(), 0));
			error("creation of new thread/vcpu failed ", res);
			return -3;
		}

		if (worker()) {
			/* local/worker threads do not require a startup portal */
			revoke(Obj_crd(pager.exc_pt_sel_client() + PT_SEL_STARTUP, 0));
		}

		pager.initial_eip((addr_t)ip);
		pager.initial_esp(initial_sp);
		pager.client_set_ec(_sel_ec());

		return 0;
	}

	if (!vcpu() && _sel_exc_base != Native_thread::INVALID_INDEX) {
		error("thread already started");
		return -5;
	}

	addr_t pd_utcb = 0;

	if (!vcpu()) {
		_sel_exc_base = 0;

		pd_utcb = stack_area_virtual_base() + stack_virtual_size() - get_page_size();

		addr_t remap_src[] = { _pd->parent_pt_sel() };
		addr_t remap_dst[] = { PT_SEL_PARENT };

		/* remap exception portals for first thread */
		for (unsigned i = 0; i < sizeof(remap_dst)/sizeof(remap_dst[0]); i++) {
			if (map_local(source_pd, utcb,
			              Obj_crd(remap_src[i], 0),
			              Obj_crd(pager.exc_pt_sel_client() + remap_dst[i], 0)))
				return -6;
		}
	}

	/* create first thread in task */
	enum { THREAD_GLOBAL = true };
	uint8_t res = create_ec(_sel_ec(), _pd->pd_sel(), kernel_cpu_id,
	                        pd_utcb, 0, _sel_exc_base,
	                        THREAD_GLOBAL);
	if (res != NOVA_OK) {
		error("create_ec returned ", res);
		return -7;
	}

	pager.client_set_ec(_sel_ec());
	pager.initial_eip((addr_t)ip);
	pager.initial_esp((addr_t)sp);

	if (vcpu())
		_features |= REMOTE_PD;
	else
		res = map_thread_portals(pager, 0, utcb);

	if (res == NOVA_OK) {
		res = syscall_retry(pager,
			[&]() {
				/* let the thread run */
				return create_sc(_sel_sc(), _pd->pd_sel(), _sel_ec(),
				                 Qpd(Qpd::DEFAULT_QUANTUM, _priority));
			});
	}

	if (res != NOVA_OK) {
		pager.client_set_ec(Native_thread::INVALID_INDEX);
		pager.initial_eip(0);
		pager.initial_esp(0);

		error("create_sc returned ", res);

		/* cap_selector free for _sel_ec is done in de-constructor */
		revoke(Obj_crd(_sel_ec(), 0));
		return -8;
	}

	_features |= SC_CREATED;

	return 0;
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

	if (!_pd || !_pager) {
		error("protection domain undefined - resuming thread failed");
		return;
	}

	uint8_t res = syscall_retry(*_pager,
		[&]() {
			return create_sc(_sel_sc(), _pd->pd_sel(), _sel_ec(),
			                 Qpd(Qpd::DEFAULT_QUANTUM, _priority));
		});

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
	if (!_pager) throw Cpu_thread::State_access_failed();

	if (!_pager->copy_thread_state(s))
		throw Cpu_thread::State_access_failed();

	/* the new state is transferred to the kernel by the recall handler */
	_pager->client_recall(false);
}


void Platform_thread::single_step(bool on)
{
	if (!_pager) return;

	_pager->single_step(on);
}

const char * Platform_thread::pd_name() const {
	return _pd ? _pd->name() : "unknown"; }


Trace::Execution_time Platform_thread::execution_time() const
{
	uint64_t sc_time = 0;
	uint64_t ec_time = 0;

	if (!sc_created()) {
		/* time executed by EC (on whatever SC) */
		uint8_t res = Nova::ec_time(_sel_ec(), ec_time);
		if (res != Nova::NOVA_OK)
			warning("ec_time failed res=", res);
		return { ec_time, sc_time, Nova::Qpd::DEFAULT_QUANTUM, _priority };
	}

	uint8_t res = Nova::sc_ec_time(_sel_sc(), _sel_ec(), sc_time, ec_time);
	if (res != Nova::NOVA_OK)
		warning("sc_ctrl failed res=", res);

	return { ec_time, sc_time, Nova::Qpd::DEFAULT_QUANTUM, _priority };
}


void Platform_thread::pager(Pager_object &pager)
{
	_pager = &pager;
	_pager->assign_pd(_pd->pd_sel());
}


void Platform_thread::thread_type(Cpu_session::Native_cpu::Thread_type thread_type,
                                  Cpu_session::Native_cpu::Exception_base exception_base)
{
	/* you can do it only once */
	if (_sel_exc_base != Native_thread::INVALID_INDEX)
		return;

	if (!main_thread() || (thread_type == Cpu_session::Native_cpu::Thread_type::VCPU))
		_sel_exc_base = exception_base.exception_base;

	if (thread_type == Cpu_session::Native_cpu::Thread_type::LOCAL)
		_features |= WORKER;
	else if (thread_type == Cpu_session::Native_cpu::Thread_type::VCPU)
		_features |= VCPU;
}


Platform_thread::Platform_thread(size_t, const char *name, unsigned prio,
                                 Affinity::Location affinity, addr_t)
:
	_pd(0), _pager(0), _id_base(cap_map().insert(2)),
	_sel_exc_base(Native_thread::INVALID_INDEX),
	_location(platform_specific().sanitize(affinity)),
	_features(0),
	_priority((uint8_t)(scale_priority(prio, name))),
	_name(name)
{ }


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
	cap_map().remove(_id_base, 2, false);
}
