/*
 * \brief  Basic test for manually managing a sub RM session
 * \author Norman Feske
 * \date   2011-11-21
 */

/*
 * Copyright (C) 2011-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <util/string.h>
#include <base/log.h>
#include <base/component.h>
#include <base/sleep.h>
#include <rm_session/connection.h>
#include <region_map/client.h>

/* platform-specific test policy */
#include <config.h>

using namespace Genode;


static void fail(char const *message)
{
	error("FAIL: ", message);
	class Test_failed{};
	throw Test_failed();
}


static char const *test_pattern() {
	return "Pattern to verify dataspace content"; }


static char const *test_pattern_2() {
	return "A second pattern to verify dataspace content"; }


static void fill_ds_with_test_pattern(Env &env, char const *pattern,
                                      Dataspace_capability ds, size_t offset)
{
	log("fill dataspace with information");
	char *content = env.rm().attach(ds);
	strncpy(content + offset, pattern, ~0);
	env.rm().detach(content);
}


static void validate_pattern_at(char const *pattern, char const *ptr)
{
	if (strcmp(ptr, pattern) != 0)
		fail("test pattern not found");
}


void Component::construct(Env &env)
{
	log("--- sub-rm test ---");

	log("create RM connection");
	enum { SUB_RM_SIZE = 1024*1024 };
	Rm_connection rm(env);

	/*
	 * Free and re-allocate the region map to excersize the 'destroy'
	 * operation.
	 */
	{
		log("create and destroy region map");
		Capability<Region_map> rm_cap = rm.create(SUB_RM_SIZE);
		rm.destroy(rm_cap);
	}

	/*
	 * Create region cap to be used for the actual test
	 */
	log("create managed dataspace");
	Region_map_client sub_rm(rm.create(SUB_RM_SIZE));
	enum { DS_SIZE = 4*4096 };
	Ram_dataspace_capability ds = env.ram().alloc(DS_SIZE);

	/*
	 * Write test patterns to the start and the second page of the RAM ds
	 */
	fill_ds_with_test_pattern(env, test_pattern(), ds, 0);
	fill_ds_with_test_pattern(env, test_pattern_2(), ds, 4096);

	if (!support_attach_sub_any) {
		log("attach RAM ds to any position at sub rm - this should fail");
		try {
			sub_rm.attach(ds, 0, 0, false, (addr_t)0);
			fail("sub rm attach_any unexpectedly did not fail");
		}
		catch (Region_map::Region_conflict) {
			log("attach failed as expected"); }
	}

	log("attach RAM ds to a fixed position at sub rm");

	enum { DS_SUB_OFFSET = 4096 };
	if ((addr_t)sub_rm.attach_at(ds, DS_SUB_OFFSET, 0, 0) != DS_SUB_OFFSET)
		fail("attach_at return-value mismatch");

	log("attach sub rm at local address space");

	/*
	 * We use a fixed local address here because this makes the address space
	 * layout more predictable. I.e., this simplifies the validation of
	 * 'proc/pid/maps' after completing the test on Linux.
	 *
	 * Technically, this could let the test fail (if Linux decides to mmap the
	 * vdso page to this location. reason ... keeping fingers crossed.
	 */
	enum { LOCAL_ATTACH_ADDR = 0x60000000 };
	char *sub_rm_base = env.rm().attach_at(sub_rm.dataspace(),
	                                       LOCAL_ATTACH_ADDR);

	log("validate pattern in sub rm");
	validate_pattern_at(test_pattern(), sub_rm_base + DS_SUB_OFFSET);

	/*
	 * Pre-populated sub rm session appeared correctly in the local address
	 * space. Now, test further populating the already attached sub rm session.
	 */

	log("attach RAM ds at another fixed position at sub rm");
	enum { DS_SUB_OFFSET_2 = 0x40000 };
	if ((addr_t)sub_rm.attach_at(ds, DS_SUB_OFFSET_2, 0, 0) != DS_SUB_OFFSET_2)
		fail("attach_at return-value mismatch");

	log("validate pattern in second mapping in sub rm");
	validate_pattern_at(test_pattern(), sub_rm_base + DS_SUB_OFFSET_2);

	/*
	 * Try to cross the boundaries of the sub RM session. This should
	 * produce an error.
	 */
	try {
		sub_rm.attach_at(ds, SUB_RM_SIZE - 4096, 0, 0);
		fail("undetected boundary conflict\n");
	}
	catch (Region_map::Region_conflict) {
		log("attaching beyond sub RM boundary failed as expected"); }

	/*
	 * Check for working region - conflict detection
	 */
	log("attaching RAM ds to a conflicting region");
	try {
		sub_rm.attach_at(ds, DS_SUB_OFFSET + 4096, 0, 0);
		fail("region conflict went undetected\n");
	}
	catch (Region_map::Region_conflict) {
		log("attaching conflicting region failed as expected"); }

	if (attach_twice_forbidden) {
		/*
		 * Try to double-attach the same sub RM session. This should fail
		 */
		log("attach sub rm again at local address space");
		try {
			env.rm().attach(sub_rm.dataspace());
			fail("double attachment of sub RM session went undetected\n");
		}
		catch (Region_map::Region_conflict) {
			log("doubly attaching sub RM session failed as expected"); }
	}

	/*
	 * Try attaching RAM ds with offset (skipping the first page of
	 * the RAM ds). We expect the second test pattern at the beginning
	 * of the region. The region size should be automatically reduced by one
	 * page.
	 */
	log("attach RAM ds with offset");
	enum { DS_SUB_OFFSET_3 = 0x80000 };
	sub_rm.attach_at(ds, DS_SUB_OFFSET_3, 0, 4096);
	validate_pattern_at(test_pattern_2(), sub_rm_base + DS_SUB_OFFSET_3);

	/*
	 * Add the size parameter to the mix, attaching only a window of two pages
	 * starting with the second page.
	 */
	log("attach RAM ds with offset and size");
	enum { DS_SUB_OFFSET_4 = 0xc0000 };
	sub_rm.attach_at(ds, DS_SUB_OFFSET_4, 2*4096, 4096);
	validate_pattern_at(test_pattern_2(), sub_rm_base + DS_SUB_OFFSET_4);

	/*
	 * Detach the first attachment (to be validated by the run script by
	 * inspecting '/proc/pid/maps' after running the test.
	 */
	sub_rm.detach((void *)DS_SUB_OFFSET);

	log("--- end of sub-rm test ---");

	/*
	 * Do not return from main function to allow the run script to inspect the
	 * memory mappings after completing the test.
	 */
	sleep_forever();
	
}
