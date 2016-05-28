/*
 * \brief  Basic test for manually managing a sub RM session
 * \author Norman Feske
 * \date   2011-11-21
 */

/*
 * Copyright (C) 2011-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#include <util/string.h>
#include <base/printf.h>
#include <base/env.h>
#include <base/sleep.h>
#include <rm_session/connection.h>
#include <region_map/client.h>

/* platform-specific test policy */
#include <config.h>

using namespace Genode;


static void fail(char const *message)
{
	PERR("FAIL: %s", message);
	class Test_failed{};
	throw Test_failed();
}


static char const *test_pattern() {
	return "Pattern to verify dataspace content"; }


static char const *test_pattern_2() {
	return "A second pattern to verify dataspace content"; }


static void fill_ds_with_test_pattern(char const *pattern,
                                      Dataspace_capability ds, size_t offset)
{
	printf("fill dataspace with information\n");
	char *content = env()->rm_session()->attach(ds);
	strncpy(content + offset, pattern, ~0);
	env()->rm_session()->detach(content);
}


static void validate_pattern_at(char const *pattern, char const *ptr)
{
	if (strcmp(ptr, pattern) != 0)
		fail("test pattern not found");
}


int main(int, char **)
{
	printf("--- sub-rm test ---\n");

	printf("create RM connection\n");
	enum { SUB_RM_SIZE = 1024*1024 };
	Rm_connection rm;

	Region_map_client sub_rm(rm.create(SUB_RM_SIZE));

	enum { DS_SIZE = 4*4096 };
	Ram_dataspace_capability ds = env()->ram_session()->alloc(DS_SIZE);

	/*
	 * Write test patterns to the start and the second page of the RAM ds
	 */
	fill_ds_with_test_pattern(test_pattern(), ds, 0);
	fill_ds_with_test_pattern(test_pattern_2(), ds, 4096);

	if (!support_attach_sub_any) {
		printf("attach RAM ds to any position at sub rm - this should fail\n");
		try {
			sub_rm.attach(ds, 0, 0, false, (addr_t)0);
			fail("sub rm attach_any unexpectedly did not fail");
		}
		catch (Region_map::Out_of_metadata) {
			printf("attach failed as expected\n"); }
	}

	printf("attach RAM ds to a fixed position at sub rm\n");

	enum { DS_SUB_OFFSET = 4096 };
	if ((addr_t)sub_rm.attach_at(ds, DS_SUB_OFFSET, 0, 0) != DS_SUB_OFFSET)
		fail("attach_at return-value mismatch");

	printf("attach sub rm at local address space\n");

	/*
	 * We use a fixed local address here because this makes the address space
	 * layout more predictable. I.e., this simplifies the validation of
	 * 'proc/pid/maps' after completing the test on Linux.
	 *
	 * Technically, this could let the test fail (if Linux decides to mmap the
	 * vdso page to this location. reason ... keeping fingers crossed.
	 */
	enum { LOCAL_ATTACH_ADDR = 0x60000000 };
	char *sub_rm_base = env()->rm_session()->attach_at(sub_rm.dataspace(),
	                                                   LOCAL_ATTACH_ADDR);

	printf("validate pattern in sub rm\n");
	validate_pattern_at(test_pattern(), sub_rm_base + DS_SUB_OFFSET);

	/*
	 * Pre-populated sub rm session appeared correctly in the local address
	 * space. Now, test further populating the already attached sub rm session.
	 */

	printf("attach RAM ds at another fixed position at sub rm\n");
	enum { DS_SUB_OFFSET_2 = 0x40000 };
	if ((addr_t)sub_rm.attach_at(ds, DS_SUB_OFFSET_2, 0, 0) != DS_SUB_OFFSET_2)
		fail("attach_at return-value mismatch");

	printf("validate pattern in second mapping in sub rm\n");
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
		printf("attaching beyond sub RM boundary failed as expected\n"); }

	/*
	 * Check for working region - conflict detection
	 */
	printf("attaching RAM ds to a conflicting region\n");
	try {
		sub_rm.attach_at(ds, DS_SUB_OFFSET + 4096, 0, 0);
		fail("region conflict went undetected\n");
	}
	catch (Region_map::Region_conflict) {
		printf("attaching conflicting region failed as expected\n"); }

	if (attach_twice_forbidden) {
		/*
		 * Try to double-attach the same sub RM session. This should fail
		 */
		printf("attach sub rm again at local address space\n");
		try {
			env()->rm_session()->attach(sub_rm.dataspace());
			fail("double attachment of sub RM session went undetected\n");
		}
		catch (Region_map::Out_of_metadata) {
			printf("doubly attaching sub RM session failed as expected\n"); }
	}

	/*
	 * Try attaching RAM ds with offset (skipping the first page of
	 * the RAM ds). We expect the second test pattern at the beginning
	 * of the region. The region size should be automatically reduced by one
	 * page.
	 */
	printf("attach RAM ds with offset\n");
	enum { DS_SUB_OFFSET_3 = 0x80000 };
	sub_rm.attach_at(ds, DS_SUB_OFFSET_3, 0, 4096);
	validate_pattern_at(test_pattern_2(), sub_rm_base + DS_SUB_OFFSET_3);

	/*
	 * Add the size parameter to the mix, attaching only a window of two pages
	 * starting with the second page.
	 */
	printf("attach RAM ds with offset and size\n");
	enum { DS_SUB_OFFSET_4 = 0xc0000 };
	sub_rm.attach_at(ds, DS_SUB_OFFSET_4, 2*4096, 4096);
	validate_pattern_at(test_pattern_2(), sub_rm_base + DS_SUB_OFFSET_4);

	/*
	 * Detach the first attachment (to be validated by the run script by
	 * inspecting '/proc/pid/maps' after running the test.
	 */
	sub_rm.detach((void *)DS_SUB_OFFSET);

	printf("--- end of sub-rm test ---\n");

	/*
	 * Do not return from main function to allow the run script to inspect the
	 * memory mappings after completing the test.
	 */
	sleep_forever();
	return 0;
}
