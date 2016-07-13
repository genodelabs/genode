/*
 * \brief  Some platform tests for the base-nova
 * \author Alexander Boettcher
 * \date   2015-01-02
 *
 */

/*
 * Copyright (C) 2015 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#include <base/thread.h>
#include <base/log.h>
#include <base/snprintf.h>

#include <util/touch.h>
#include <rm_session/connection.h>
#include <region_map/client.h>

#include <os/attached_rom_dataspace.h>
#include <os/config.h>

#include <trace/timestamp.h>
#include <nova/native_thread.h>

#include "server.h"

static unsigned failed = 0;

static unsigned check_pat = 1;

static Genode::Cap_connection cap;

using namespace Genode;

void test_translate()
{
	enum { STACK_SIZE = 4096 };
	static Rpc_entrypoint ep(&cap, STACK_SIZE, "rpc_ep_translate");

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

void test_revoke()
{
	enum { STACK_SIZE = 4096 };
	static Rpc_entrypoint ep(&cap, STACK_SIZE, "rpc_ep_revoke");

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

	rpc = Test::cap_void_manual(copy_session_cap, copy_session_cap, local_name);
	if (rpc != Genode::Rpc_exception_code::SUCCESS ||
	    local_name == (addr_t)copy_session_cap.local_name() ||
	    local_name == (addr_t)Native_thread::INVALID_INDEX)
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
	Genode::Native_capability pager_cap = Capability_space::import(myself->native_thread().ec_sel + 1);
	request_event_portal(pager_cap, copy_session_cap.local_name(), 0, 0);

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

void test_pat()
{
	/* read out the tsc frequenzy once */
	Genode::Attached_rom_dataspace _ds("hypervisor_info_page");
	Nova::Hip * const hip = _ds.local_addr<Nova::Hip>();

	enum { DS_ORDER = 12, PAGE_4K = 12 };

	Ram_dataspace_capability ds = env()->ram_session()->alloc (1 << (DS_ORDER + PAGE_4K), WRITE_COMBINED);
	addr_t map_addr = env()->rm_session()->attach(ds);

	enum { STACK_SIZE = 4096 };

	static Rpc_entrypoint ep(&cap, STACK_SIZE, "rpc_ep_pat");

	Test::Component  component;
	Test::Capability session_cap = ep.manage(&component);
	Test::Client     client(session_cap);

	Genode::Rm_connection rm;
	Genode::Region_map_client rm_free_area(rm.create(1 << (DS_ORDER + PAGE_4K)));
	addr_t remap_addr = Genode::env()->rm_session()->attach(rm_free_area.dataspace());

	/* trigger mapping of whole area */
	for (addr_t i = map_addr; i < map_addr + (1 << (DS_ORDER + PAGE_4K)); i += (1 << PAGE_4K))
		touch_read(reinterpret_cast<unsigned char *>(map_addr));

	/*
	 * Manipulate entrypoint
	 */
	Nova::Rights all(true, true, true);
	Genode::addr_t  utcb_ep_addr_t = client.leak_utcb_address();
	Nova::Utcb *utcb_ep = reinterpret_cast<Nova::Utcb *>(utcb_ep_addr_t);
	/* overwrite receive window of entrypoint */
	utcb_ep->crd_rcv = Nova::Mem_crd(remap_addr >> PAGE_4K, DS_ORDER, all);

	/*
	 * Set-up current (client) thread to delegate write-combined memory
	 */
	Nova::Mem_crd snd_crd(map_addr >> PAGE_4K, DS_ORDER, all);

	Nova::Utcb *utcb = reinterpret_cast<Nova::Utcb *>(Thread::myself()->utcb());
	enum {
		HOTSPOT = 0, USER_PD = false, HOST_PGT = false, SOLELY_MAP = false,
		NO_DMA = false, EVILLY_DONT_WRITE_COMBINE = false
	};

	Nova::Crd old = utcb->crd_rcv;

	utcb->set_msg_word(0);
	bool ok = utcb->append_item(snd_crd, HOTSPOT, USER_PD, HOST_PGT,
	                            SOLELY_MAP, NO_DMA, EVILLY_DONT_WRITE_COMBINE);
	(void)ok;

	uint8_t res = Nova::call(session_cap.local_name());
	(void)res;

	utcb->crd_rcv = old;

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

	if (check_pat && diff_run * 100 / hip->tsc_freq) {
		failed ++;

		error("map=", Hex(map_run), " remap=", Hex(remap_run), " --> "
		      "diff=", Hex(diff_run), " freq_tsc=", hip->tsc_freq, " ",
		     diff_run * 1000 / hip->tsc_freq, " us");
	}

	Nova::revoke(Nova::Mem_crd(remap_addr >> PAGE_4K, DS_ORDER, all));

	/*
	 * note: server entrypoint died because of unexpected receive window
	 *       state - that is expected
	 */
}

void test_server_oom()
{
	using namespace Genode;

	enum { STACK_SIZE = 4096 };

	static Rpc_entrypoint ep(&cap, STACK_SIZE, "rpc_ep_oom");

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
		Cap_index idx(cap_map()->find(got_cap.local_name()));
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

class Greedy : public Thread_deprecated<4096> {

	public:

		Greedy()
		:
			Thread_deprecated<0x1000>("greedy")
		{ }

		void entry()
		{
			log("starting");

			enum { SUB_RM_SIZE = 2UL * 1024 * 1024 * 1024 };

			Genode::Rm_connection rm;
			Genode::Region_map_client sub_rm(rm.create(SUB_RM_SIZE));
			addr_t const mem = env()->rm_session()->attach(sub_rm.dataspace());

			Nova::Utcb * nova_utcb = reinterpret_cast<Nova::Utcb *>(utcb());
			Nova::Rights const mapping_rwx(true, true, true);

			addr_t const page_fault_portal = native_thread().exc_pt_sel + 14;

			log("cause mappings in range ",
			    Hex_range<addr_t>(mem, SUB_RM_SIZE), " ", &mem);

			for (addr_t map_to = mem; map_to < mem + SUB_RM_SIZE; map_to += 4096) {

				/* setup faked page fault information */
				nova_utcb->items   = ((addr_t)&nova_utcb->qual[2] - (addr_t)nova_utcb->msg) / sizeof(addr_t);
				nova_utcb->ip      = 0xbadaffe;
				nova_utcb->qual[1] = (addr_t)&mem;
				nova_utcb->crd_rcv = Nova::Mem_crd(map_to >> 12, 0, mapping_rwx);

				/* trigger faked page fault */
				Genode::uint8_t res = Nova::call(page_fault_portal);
				if (res != Nova::NOVA_OK) {
					log("call result=", res);
					failed++;
					return;
				}

				/* check that we really got the mapping */
				touch_read(reinterpret_cast<unsigned char *>(map_to));

				/* print status information in interval of 32M */
				if (!(map_to & (32UL * 1024 * 1024 - 1))) {
					log(Hex(map_to));
					/* trigger some work to see quota in kernel decreasing */
//					Nova::Rights rwx(true, true, true);
//					Nova::revoke(Nova::Mem_crd((map_to - 32 * 1024 * 1024) >> 12, 12, rwx));
				}
			}
			log("still alive - done");
		}
};

void check(uint8_t res, const char *format, ...)
{
	static char buf[128];

	va_list list;
	va_start(list, format);

	String_console sc(buf, sizeof(buf));
	sc.vprintf(format, list);

	va_end(list);

	if (res == Nova::NOVA_OK) {
		error("res=", res, " ", Cstring(buf), " - TEST FAILED");
		failed++;
	}
	else
		log("res=", res, " ", Cstring(buf));
}

int main(int argc, char **argv)
{
	log("testing base-nova platform");

	try {
		Genode::config()->xml_node().attribute("check_pat").value(&check_pat);
	} catch (...) { }

	Thread * myself = Thread::myself();
	if (!myself)
		return -__LINE__;

	addr_t sel_pd  = cap_map()->insert();
	addr_t sel_ec  = myself->native_thread().ec_sel;
	addr_t sel_cap = cap_map()->insert();
	addr_t handler = 0UL;
	uint8_t    res = 0;

	Nova::Mtd mtd(Nova::Mtd::ALL);

	if (sel_cap == ~0UL || sel_ec == ~0UL || sel_cap == ~0UL)
		return -__LINE__;

	/* negative syscall tests - they should not succeed */
	res = Nova::create_pt(sel_cap, sel_pd, sel_ec, mtd, handler);
	check(res, "create_pt");

	res = Nova::create_sm(sel_cap, sel_pd, 0);
	check(res, "create_sm");

	/* changing the badge of one of the portal must fail */
	for (unsigned i = 0; i < (1U << Nova::NUM_INITIAL_PT_LOG2); i++) {
		addr_t sel_exc = myself->native_thread().exc_pt_sel + i;
		res = Nova::pt_ctrl(sel_exc, 0xbadbad);
		check(res, "pt_ctrl %2u", i);
	}

	/* upgrade available capability indices for this process */
	unsigned index = 512 * 1024;
	static char local[128][sizeof(Cap_range)];

	for (unsigned i = 0; i < sizeof(local) / sizeof (local[0]); i++) {
		Cap_range * range = reinterpret_cast<Cap_range *>(local[i]);
		*range = Cap_range(index);

		cap_map()->insert(range);

		index = range->base() + range->elements();
	};

	/* test PAT kernel feature */
	test_pat();

	/* test special revoke */
	test_revoke();

	/* test translate together with special revoke */
	test_translate();

	/**
	 * Test to provoke out of memory during capability transfer of
	 * server/client.
	 *
	 * Set in hypervisor.ld the memory to a low value of about 1M to let
	 * trigger the test.
	 */
	test_server_oom();

	/* Test to provoke out of memory in kernel during interaction with core */
	static Greedy core_pagefault_oom;
	core_pagefault_oom.start();
	core_pagefault_oom.join();

	if (!failed)
		log("Test finished");

	return -failed;
}
