/*
 * \brief  Some platform tests for the base-nova
 * \author Alexander Boettcher
 * \date   2015-01-02
 *
 */

/*
 * Copyright (C) 2015-2020 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <base/heap.h>
#include <base/thread.h>
#include <base/log.h>
#include <base/component.h>
#include <util/touch.h>
#include <util/retry.h>
#include <rm_session/connection.h>
#include <region_map/client.h>

#include <base/attached_rom_dataspace.h>

#include <trace/timestamp.h>

#include <nova/native_thread.h>
#include <nova_native_pd/client.h>

#include "server.h"

static unsigned failed = 0;

static unsigned check_pat = 1;

using namespace Genode;

void test_translate(Genode::Env &env)
{
	enum { STACK_SIZE = 4096 };
	static Rpc_entrypoint ep(&env.pd(), STACK_SIZE, "rpc_ep_translate",
	                         Affinity::Location());

	Test::Component  component;
	Test::Capability session_cap = ep.manage(&component);
	Test::Client     client(session_cap);

	Genode::addr_t local_name = Native_thread::INVALID_INDEX;

	long rpc = Test::cap_void_manual(session_cap, session_cap, local_name);
	if (rpc != Genode::Rpc_exception_code::SUCCESS ||
	    local_name == (addr_t)session_cap.local_name() ||
	    local_name == (addr_t)Native_thread::INVALID_INDEX)
	{
		failed ++;
		error(__func__, ": ipc call failed ", Hex(rpc));
		ep.dissolve(&component);
		return;
	}

	Genode::Native_capability copy1 = Capability_space::import(local_name);

	rpc = Test::cap_void_manual(session_cap, copy1, local_name);
	if (rpc != Genode::Rpc_exception_code::SUCCESS ||
	    local_name == (addr_t)copy1.local_name() ||
	    local_name == (addr_t)Native_thread::INVALID_INDEX)
	{
		failed ++;
		error(__func__, ": ipc call failed ", Hex(rpc));
		ep.dissolve(&component);
		return;
	}

	Genode::Native_capability copy2 = Capability_space::import(local_name);

	log("delegation session_cap->copy1->copy2 ",
	    session_cap, "->", copy1, "->", copy2);

	/* sanity checks translate which must work */
	Genode::Native_capability got_cap = client.cap_cap(copy2.local_name());
	if (got_cap.local_name() != copy1.local_name()) {
		failed ++;
		error(__LINE__, ":", __func__, " translate failed");
		ep.dissolve(&component);
		return;
	}

	got_cap = client.cap_cap(copy1.local_name());
	if (got_cap.local_name() != session_cap.local_name()) {
		failed ++;
		error(__LINE__, ":", __func__, " translate failed");
		ep.dissolve(&component);
		return;
	}

	got_cap = client.cap_cap(session_cap.local_name());
	if (got_cap.local_name() != session_cap.local_name()) {
		failed ++;
		error(__LINE__, ":", __func__, " translate failed");
		ep.dissolve(&component);
		return;
	}

	/**
	 * Test special revoke by make the intermediate cap (copy1) inaccessible
	 * and check that translate of copy2 get the right results.
	 */

	Nova::Obj_crd crd_ses(copy1.local_name(), 0);
	enum { SELF = true, LOCAL_REVOKE = false, LOCAL_PD = 0, NO_BLOCKING = 0, KEEP_IN_MDB = true };
	Nova::revoke(crd_ses, SELF, LOCAL_REVOKE, LOCAL_PD, NO_BLOCKING, KEEP_IN_MDB);

	crd_ses = Nova::Obj_crd(copy1.local_name(), 0);
	Genode::uint8_t res = Nova::lookup(crd_ses);
	if (res != Nova::NOVA_OK || !crd_ses.is_null()) {
		failed ++;
		error(__LINE__, " - lookup call failed err=", Hex(res));
		ep.dissolve(&component);
		return;
	}

	/* copy1 should be skipped and session_cap is the valid response */
	got_cap = client.cap_cap(copy2.local_name());
	if (got_cap.local_name() != session_cap.local_name()) {
		failed ++;
		error(__LINE__, ":", __func__, " translate failed");
		ep.dissolve(&component);
		return;
	}

	ep.dissolve(&component);
}

void test_revoke(Genode::Env &env)
{
	enum { STACK_SIZE = 4096 };
	static Rpc_entrypoint ep(&env.pd(), STACK_SIZE, "rpc_ep_revoke",
	                         Affinity::Location());

	Test::Component  component;
	Test::Capability session_cap = ep.manage(&component);
	Test::Client     client(session_cap);

	Genode::addr_t local_name = Native_thread::INVALID_INDEX;

	long rpc = Test::cap_void_manual(session_cap, session_cap, local_name);
	if (rpc != Genode::Rpc_exception_code::SUCCESS ||
	    local_name == (addr_t)session_cap.local_name() ||
	    local_name == (addr_t)Native_thread::INVALID_INDEX)
	{
		failed ++;
		error("test_revoke ipc call failed ", Hex(rpc));
		ep.dissolve(&component);
		return;
	}

	Genode::Native_capability copy_session_cap = Capability_space::import(local_name);

	local_name = Native_thread::INVALID_INDEX;
	rpc = Test::cap_void_manual(copy_session_cap, copy_session_cap, local_name);
	if (rpc != Genode::Rpc_exception_code::SUCCESS ||
	    local_name == (addr_t)copy_session_cap.local_name() ||
	    local_name == (addr_t)Native_thread::INVALID_INDEX ||
	    local_name == (addr_t)session_cap.local_name())
	{
		failed ++;
		error("test_revoke ipc call failed ", Hex(rpc));
		ep.dissolve(&component);
		return;
	}

	Nova::Obj_crd crd_dst(local_name, 0);
	Genode::uint8_t res = Nova::lookup(crd_dst);
	if (res != Nova::NOVA_OK || crd_dst.base() != local_name || crd_dst.type() != 3 ||
	    crd_dst.order() != 0) {
		failed ++;
		error(__LINE__, " - lookup call failed ", Hex(res));
		ep.dissolve(&component);
		return;
	}

	Nova::Obj_crd crd_ses(copy_session_cap.local_name(), 0);
	res = Nova::lookup(crd_ses);
	if (res != Nova::NOVA_OK || crd_ses.base() != (addr_t)copy_session_cap.local_name()
	 || crd_ses.type() != 3 || crd_ses.order() != 0) {
		failed ++;
		error(__LINE__, " - lookup call failed err=", Hex(res), " is_null=", crd_ses.is_null());
		ep.dissolve(&component);
		return;
	}

	res = Nova::lookup(crd_dst);
	if (res != Nova::NOVA_OK || crd_dst.base() != local_name || crd_dst.type() != 3 ||
	    crd_dst.order() != 0) {
		failed ++;
		error(__LINE__, " - lookup call failed err=", Hex(res), " is_null=", crd_dst.is_null());
		ep.dissolve(&component);
		return;
	}

	crd_ses = Nova::Obj_crd(copy_session_cap.local_name(), 0);
	enum { SELF = true, LOCAL_REVOKE = false, LOCAL_PD = 0, NO_BLOCKING = 0, KEEP_IN_MDB = true };
	Nova::revoke(crd_ses, SELF, LOCAL_REVOKE, LOCAL_PD, NO_BLOCKING, KEEP_IN_MDB);

	crd_ses = Nova::Obj_crd(copy_session_cap.local_name(), 0);
	res = Nova::lookup(crd_ses);
	if (res != Nova::NOVA_OK || !crd_ses.is_null()) {
		failed ++;
		error(__LINE__, " - lookup call failed err=", Hex(res));
		ep.dissolve(&component);
		return;
	}

	res = Nova::lookup(crd_dst);
	if (res != Nova::NOVA_OK || crd_dst.base() != local_name || crd_dst.type() != 3 ||
	    crd_dst.order() != 0) {
		failed ++;
		error(__LINE__, " - lookup call failed err=", Hex(res), " is_null=", crd_dst.is_null());
		ep.dissolve(&component);
		return;
	}

	/*
	 * Request some other capability and place it on very same selector
	 * as used before by copy_session_cap
	 */
	Genode::Thread * myself = Genode::Thread::myself();
	request_native_ec_cap(myself->native_thread().exc_pt_sel + Nova::PT_SEL_PAGE_FAULT,
	                      copy_session_cap.local_name());

	/* check whether the requested cap before is valid and placed well */
	crd_ses = Nova::Obj_crd(copy_session_cap.local_name(), 0);
	res = Nova::lookup(crd_ses);
	if (res != Nova::NOVA_OK || crd_ses.base() != (addr_t)copy_session_cap.local_name() ||
	    crd_ses.type() != 3 || crd_ses.order() != 0) {
		failed ++;
		error(__LINE__, " - lookup call failed err=", Hex(res), " is_null=", crd_ses.is_null());
		ep.dissolve(&component);
		return;
	}

	/* revoke it */
	Nova::revoke(crd_ses, SELF, LOCAL_REVOKE, LOCAL_PD, NO_BLOCKING);

	/* the delegated cap to the client should still be there */
	res = Nova::lookup(crd_dst);
	if (res != Nova::NOVA_OK || crd_dst.base() != local_name || crd_dst.type() != 3 ||
	    crd_dst.order() != 0) {
		failed ++;
		error(__LINE__, " - lookup call failed err=", Hex(res), " is_null=", crd_dst.is_null());
		ep.dissolve(&component);
		return;
	}

	/* kill the original session capability */
	ep.dissolve(&component);
	/* manually: cap.free_rpc_cap(session_cap); */

	/* the delegated cap to the client should be now invalid */
	res = Nova::lookup(crd_dst);
	if (res != Nova::NOVA_OK || !crd_dst.is_null()) {
		failed ++;
		error(__LINE__, " - lookup call failed err=", Hex(res), " is_null=", crd_dst.is_null());
		return;
	}
}

static void portal_entry()
{
	Genode::Thread &myself = *Genode::Thread::myself();
	Nova::Utcb &utcb = *reinterpret_cast<Nova::Utcb *>(myself.utcb());

	Nova::Crd const snd_crd(utcb.msg()[0]);

	enum {
		HOTSPOT = 0, USER_PD = false, HOST_PGT = false, SOLELY_MAP = false,
		NO_DMA = false, EVILLY_DONT_WRITE_COMBINE = false
	};

	utcb.set_msg_word(0);
	bool ok = utcb.append_item(snd_crd, HOTSPOT, USER_PD, HOST_PGT,
	                           SOLELY_MAP, NO_DMA, EVILLY_DONT_WRITE_COMBINE);
	(void)ok;

	Nova::reply(myself.stack_top());
}

void test_pat(Genode::Env &env)
{
	Genode::Thread &myself = *Genode::Thread::myself();
	Nova::Utcb &utcb = *reinterpret_cast<Nova::Utcb *>(myself.utcb());

	/* read out the tsc frequenzy once */
	Attached_rom_dataspace const platform_info (env, "platform_info");
	Xml_node const hardware = platform_info.xml().sub_node("hardware");
	uint64_t const tsc_freq = hardware.sub_node("tsc").attribute_value("freq_khz", 1ULL);

	enum { DS_ORDER = 12, PAGE_4K = 12 };

	Ram_dataspace_capability ds = env.ram().alloc (1 << (DS_ORDER + PAGE_4K),
	                                               WRITE_COMBINED);
	addr_t map_addr = env.rm().attach(ds);

	enum { STACK_SIZE = 4096 };

	static Rpc_entrypoint ep(&env.pd(), STACK_SIZE, "rpc_ep_pat",
	                         Affinity::Location());

	Genode::Rm_connection rm(env);
	Genode::Region_map_client rm_free_area(rm.create(1 << (DS_ORDER + PAGE_4K)));
	addr_t remap_addr = env.rm().attach(rm_free_area.dataspace());

	/* trigger mapping of whole area */
	for (addr_t i = map_addr; i < map_addr + (1 << (DS_ORDER + PAGE_4K)); i += (1 << PAGE_4K))
		touch_read(reinterpret_cast<unsigned char *>(map_addr));

	/*
	 * Establish memory mapping with evilly wrong mapping attributes
	 */
	Nova_native_pd_client native_pd { env.pd().native_pd() };
	Thread * thread = reinterpret_cast<Genode::Thread *>(&ep);
	Native_capability const thread_cap
		= Capability_space::import(thread->native_thread().ec_sel);

	Untyped_capability const pt =
		native_pd.alloc_rpc_cap(thread_cap, (addr_t)portal_entry, 0 /* MTD */);

	Nova::Rights  const all(true, true, true);
	Nova::Mem_crd const rcv_crd(remap_addr >> PAGE_4K, DS_ORDER, all);
	Nova::Mem_crd const snd_crd(map_addr >> PAGE_4K, DS_ORDER, all);
	Nova::Crd     const old_crd = utcb.crd_rcv;

	utcb.crd_rcv = rcv_crd;
	utcb.set_msg_word(1);
	utcb.msg()[0] = snd_crd.value();

	uint8_t const res = Nova::call(pt.local_name());
	utcb.crd_rcv = old_crd;

	if (res != Nova::NOVA_OK) {
		Genode::error("establishing memory failed ", res);
		failed++;
	}

	/* sanity check - touch re-mapped area */
	for (addr_t i = remap_addr; i < remap_addr + (1 << (DS_ORDER + PAGE_4K)); i += (1 << PAGE_4K))
		touch_read(reinterpret_cast<unsigned char *>(remap_addr));

	/*
	 * measure time to write to the memory
	 */
	memset(reinterpret_cast<void *>(map_addr), 0, 1 << (DS_ORDER + PAGE_4K));
	Trace::Timestamp map_start = Trace::timestamp();
	memset(reinterpret_cast<void *>(map_addr), 0, 1 << (DS_ORDER + PAGE_4K));
	Trace::Timestamp map_end = Trace::timestamp();

	memset(reinterpret_cast<void *>(remap_addr), 0, 1 << (DS_ORDER + PAGE_4K));
	Trace::Timestamp remap_start = Trace::timestamp();
	memset(reinterpret_cast<void *>(remap_addr), 0, 1 << (DS_ORDER + PAGE_4K));
	Trace::Timestamp remap_end = Trace::timestamp();

	Trace::Timestamp map_run   = map_end - map_start;
	Trace::Timestamp remap_run = remap_end - remap_start;

	Trace::Timestamp diff_run = map_run > remap_run ? map_run - remap_run : remap_run - map_run;

	if (check_pat && diff_run * 100 / tsc_freq) {
		failed ++;

		error("map=", Hex(map_run), " remap=", Hex(remap_run), " --> "
		      "diff=", Hex(diff_run), " freq_tsc=", tsc_freq, " ",
		     diff_run * 1000 / tsc_freq, " us");
	}

	Nova::revoke(Nova::Mem_crd(remap_addr >> PAGE_4K, DS_ORDER, all));
}

void test_server_oom(Genode::Env &env)
{
	using namespace Genode;

	enum { STACK_SIZE = 4096 };

	static Rpc_entrypoint ep(&env.pd(), STACK_SIZE, "rpc_ep_oom",
	                         Affinity::Location());

	Test::Component  component;
	Test::Capability session_cap = ep.manage(&component);
	Test::Client     client(session_cap);

	/* case that during reply we get oom */
	for (unsigned i = 0; i < 20000; i++) {
		Genode::Native_capability got_cap = client.void_cap();

		if (!got_cap.valid()) {
			error(i, " cap id ", Hex(got_cap.local_name()), " invalid");
			failed ++;
			break;
		}

		/* be evil and keep this cap by manually incrementing the ref count */
		Cap_index idx(cap_map().find(got_cap.local_name()));
		idx.inc();

		if (i % 5000 == 4999)
			log("received ", i, ". cap");
	}

	/* XXX this code does does no longer work since the removal of 'solely_map' */
#if 0

	/* case that during send we get oom */
	for (unsigned i = 0; i < 20000; i++) {
		/* be evil and switch translation off - server ever uses a new selector */
		Genode::Native_capability send_cap = session_cap;
		send_cap.solely_map();

		if (!client.cap_void(send_cap)) {
			error("sending ", i, ". cap failed");
			failed ++;
			break;
		}

		if (i % 5000 == 4999)
			log("sent ", i, ". cap");
	}
#endif

	ep.dissolve(&component);
}

class Pager : private Genode::Thread {

	private:

		Native_capability _call_to_map { };
		Ram_dataspace_capability _ds;
		static addr_t _ds_mem;

		void entry() override { }

		static void page_fault()
		{
			Thread     * myself  = Thread::myself();
			Nova::Utcb * utcb    = reinterpret_cast<Nova::Utcb *>(myself->utcb());

			if (utcb->msg_words() != 1) {
				Genode::error("unexpected");
				while (1) { }
			}

			Genode::addr_t map_from = utcb->msg()[0];
//			Genode::error("pager: got map request ", Genode::Hex(map_from));

			utcb->set_msg_word(0);
			utcb->mtd = 0;

			Nova::Mem_crd crd_map(map_from >> 12, 0, Nova::Rights(true, true, true));
			bool res = utcb->append_item(crd_map, 0);
			(void)res;

			Nova::reply(myself->stack_top());
		}

	public:

		Pager(Genode::Env &env, Location location)
		:
			Thread(env, "pager", 0x1000, location, Weight(), env.cpu()),
			_ds(env.ram().alloc (4096))
		{
			_ds_mem = env.rm().attach(_ds);
			touch_read(reinterpret_cast<unsigned char *>(_ds_mem));

			/* request creation of a 'local' EC */
			Thread::native_thread().ec_sel = Native_thread::INVALID_INDEX - 1;
			Thread::start();

			Genode::warning("pager: created");

			Native_capability thread_cap =
				Capability_space::import(Thread::native_thread().ec_sel);

			Genode::Nova_native_pd_client native_pd(env.pd().native_pd());
			Nova::Mtd mtd (Nova::Mtd::QUAL | Nova::Mtd::EIP | Nova::Mtd::ESP);
			Genode::addr_t entry = reinterpret_cast<Genode::addr_t>(page_fault);

			_call_to_map = native_pd.alloc_rpc_cap(thread_cap, entry,
			                                       mtd.value());
		}

		Native_capability call_to_map() { return _call_to_map; }
		addr_t mem_st() { return _ds_mem; }
};

addr_t Pager::_ds_mem;

class Cause_mapping : public Genode::Thread {

	private:

		Native_capability  _call_to_map { };
		Rm_connection      _rm;
		Region_map_client  _sub_rm;
		addr_t             _mem_nd;
		addr_t             _mem_st;
		Nova::Rights const _mapping_rwx = {true, true, true};

	public:

		unsigned volatile called = 0;

		Cause_mapping(Genode::Env &env, Native_capability call_to_map,
		              Genode::addr_t mem_st, Location location)
		:
			Thread(env, "mapper", 0x1000, location, Weight(), env.cpu()),
			_call_to_map(call_to_map),
			_rm(env),
			_sub_rm(_rm.create(0x2000)),
			_mem_nd(env.rm().attach(_sub_rm.dataspace())),
			_mem_st(mem_st)
		{ }

		void entry() override
		{
			log("mapper: hello");

			Nova::Utcb * nova_utcb = reinterpret_cast<Nova::Utcb *>(utcb());

			while (true) {
				called = called + 1;
//				log("mapper: request mapping ", Hex(_mem_nd), " ", called);

				Nova::Crd old = nova_utcb->crd_rcv;

//				touch_read((unsigned char *)_mem_st);

				nova_utcb->msg()[0] = _mem_st;
				nova_utcb->set_msg_word(1);
				nova_utcb->crd_rcv = Nova::Mem_crd(_mem_nd >> 12, 0,
				                                   _mapping_rwx);
				Nova::call(_call_to_map.local_name());
				//touch_read((unsigned char *)_mem_nd);

				nova_utcb->msg()[0] = _mem_nd;
				nova_utcb->set_msg_word(1);
				nova_utcb->crd_rcv = Nova::Mem_crd((_mem_nd + 0x1000) >> 12, 0,
				                                   _mapping_rwx);
				Nova::call(_call_to_map.local_name());
//				touch_read((unsigned char *)_mem_nd + 0x1000);

				nova_utcb->crd_rcv = old;
			}
		}

		void revoke_remote()
		{
			Nova::revoke(Nova::Mem_crd(_mem_nd >> 12, 0, _mapping_rwx), true);
		}
};

void test_delegate_revoke_smp(Genode::Env &env)
{
	Affinity::Space cpus = env.cpu().affinity_space();
	Genode::log("detected ", cpus.width(), "x", cpus.height(), " "
	            "CPU", cpus.total() > 1 ? "s." : ".");

	Pager pager(env, cpus.location_of_index(1));
	Cause_mapping mapper(env, pager.call_to_map(), pager.mem_st(),
	                     cpus.location_of_index(1));
	mapper.start();

	for (unsigned i = 0; i < 2000; i++) {
		mapper.revoke_remote();
		if (i % 1000 == 0)
			Genode::log("main ", i, " ", mapper.called);
	}
}

class Greedy : public Genode::Thread {

	private:

		Genode::Env &_env;

	public:

		Greedy(Genode::Env &env)
		:
			Thread(env, "greedy", 0x1000),
			_env(env)
		{ }

		void entry() override
		{
			log("starting");

			enum { SUB_RM_SIZE = 1280 * 1024 * 1024 };

			Genode::Ram_dataspace_capability ds = _env.ram().alloc(4096);

			Nova::Rights const mapping_rwx(true, true, true);

			log("cause mappings");

			for (unsigned i = 0; i < SUB_RM_SIZE / 4096; i++) {

				addr_t map_to = _env.rm().attach(ds);

				/* check that we really got the mapping */
				touch_read(reinterpret_cast<unsigned char *>(map_to));

				/* print status information in interval of 32M */
				if (i % 8192 == 0) {
					log(Hex(i * 4096));
					/* trigger some work to see quota in kernel decreasing */
//					Nova::Rights rwx(true, true, true);
//					Nova::revoke(Nova::Mem_crd((map_to - 32 * 1024 * 1024) >> 12, 12, rwx));
				}
			}
			log("still alive - done");
		}
};


template <typename... ARGS>
void check(uint8_t res, ARGS &&... args)
{
	String<128> msg(args...);

	if (res == Nova::NOVA_OK) {
		error("res=", res, " ", msg, " - TEST FAILED");
		failed++;
	}
	else
		log("res=", res, " ", msg);
}

struct Main
{
	Genode::Env  &env;
	Genode::Heap  heap { env.ram(), env.rm() };

	Main(Env &env);
};

Main::Main(Env &env) : env(env)
{
	log("testing base-nova platform");

	try {
		Attached_rom_dataspace config(env, "config");
		config.xml().attribute("check_pat").value(check_pat);
	} catch (...) {
		Genode::error("no check_pat attribute found");
		env.parent().exit(-__LINE__);
		return;
	}

	Thread * myself = Thread::myself();
	if (!myself) {
		env.parent().exit(-__LINE__);
		return;
	}

	/* upgrade available capability indices for this process */
	addr_t index = 512 * 1024;
	static char local[128][sizeof(Cap_range)];

	for (addr_t i = 0; i < sizeof(local) / sizeof (local[0]); i++) {
		Cap_range &range = *construct_at<Cap_range>(local[i], index);

		cap_map().insert(range);

		index = range.base() + range.elements();
	};

	addr_t sel_pd  = cap_map().insert();
	addr_t sel_ec  = myself->native_thread().ec_sel;
	addr_t sel_cap = cap_map().insert();
	addr_t handler = 0UL;
	uint8_t    res = 0;

	Nova::Mtd mtd(Nova::Mtd::ALL);

	if (sel_cap == ~0UL || sel_ec == ~0UL || sel_cap == ~0UL) {
		env.parent().exit(-__LINE__);
		return;
	}

	/* negative syscall tests - they should not succeed */
	res = Nova::create_pt(sel_cap, sel_pd, sel_ec, mtd, handler);
	check(res, "create_pt");

	res = Nova::create_sm(sel_cap, sel_pd, 0);
	check(res, "create_sm");

	/* changing the badge of one of the portal must fail */
	for (unsigned i = 0; i < (1U << Nova::NUM_INITIAL_PT_LOG2); i++) {
		addr_t sel_exc = myself->native_thread().exc_pt_sel + i;
		res = Nova::pt_ctrl(sel_exc, 0xbadbad);
		check(res, "pt_ctrl ", i);
	}

	/* test PAT kernel feature */
	test_pat(env);

	/* test special revoke */
	test_revoke(env);

	/* test translate together with special revoke */
	test_translate(env);

	/* test SMP delegate/revoke - skip it on Qemu which takes too long */
	if (check_pat)
		test_delegate_revoke_smp(env);

	/**
	 * Test to provoke out of memory during capability transfer of
	 * server/client.
	 *
	 * Set in hypervisor.ld the memory to a low value of about 1M to let
	 * trigger the test.
	 */
	test_server_oom(env);

	/* Test to provoke out of memory in kernel during interaction with core */
	static Greedy core_pagefault_oom(env);
	core_pagefault_oom.start();
	core_pagefault_oom.join();

	if (!failed)
		log("Test finished");

	env.parent().exit(-__LINE__);
}


void Component::construct(Genode::Env &env) { static Main main(env); }
