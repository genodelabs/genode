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
#include <base/attached_rom_dataspace.h>
#include <base/sleep.h>
#include <rm_session/connection.h>
#include <region_map/client.h>

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
                                      Dataspace_capability ds_cap, size_t offset)
{
	log("fill dataspace with information");
	Attached_dataspace ds { env.rm(), ds_cap };
	copy_cstring(ds.local_addr<char>() + offset, pattern, ~0);
}


static void validate_pattern_at(char const *pattern, char const *ptr)
{
	if (strcmp(ptr, pattern) != 0)
		fail("test pattern not found");
}


void Component::construct(Env &env)
{
	Attached_rom_dataspace config(env, "config");

	log("--- sub-rm test ---");

	log("create RM connection");
	size_t const SUB_RM_SIZE = 1024*1024;
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
	size_t const DS_SIZE = 4*4096;
	Ram_dataspace_capability ds = env.ram().alloc(DS_SIZE);

	/*
	 * Write test patterns to the start and the second page of the RAM ds
	 */
	fill_ds_with_test_pattern(env, test_pattern(), ds, 0);
	fill_ds_with_test_pattern(env, test_pattern_2(), ds, 4096);

	if (!config.xml().attribute_value("support_attach_sub_any", true)) {
		log("attach RAM ds to any position at sub rm - this should fail");
		sub_rm.attach(ds, {
			.size       = { },  .offset    = { },
			.use_at     = { },  .at        = { },
			.executable = { },  .writeable = true
		}).with_result(
			[&] (Region_map::Range) {
				fail("sub rm attach_any unexpectedly did not fail"); },
			[&] (Region_map::Attach_error e) {
				if (e == Region_map::Attach_error::REGION_CONFLICT)
					log("attach failed as expected"); }
		);
	}

	log("attach RAM ds to a fixed position at sub rm");

	addr_t const DS_SUB_OFFSET = 4096;
	sub_rm.attach(ds, {
		.size       = { },   .offset    = { },
		.use_at     = true,  .at        = DS_SUB_OFFSET,
		.executable = { },   .writeable = { }
	}).with_result(
		[&] (Region_map::Range const range) {
			if (range.start != DS_SUB_OFFSET)
				fail("attach-at return-value mismatch"); },
		[&] (Region_map::Attach_error) { }
	);

	log("attach sub rm at local address space");

	/*
	 * We use a fixed local address here because this makes the address space
	 * layout more predictable. I.e., this simplifies the validation of
	 * 'proc/pid/maps' after completing the test on Linux.
	 *
	 * Technically, this could let the test fail (if Linux decides to mmap the
	 * vdso page to this location. reason ... keeping fingers crossed.
	 */
	addr_t const local_attach_addr =
		config.xml().attribute_value("local_attach_addr", (addr_t)0);

	char * const sub_rm_base = env.rm().attach(sub_rm.dataspace(), {
		.size       = { },   .offset    = { },
		.use_at     = true,  .at        = local_attach_addr,
		.executable = { },   .writeable = true
	}).convert<char *>(
		[&] (Region_map::Range const range) { return (char *)range.start; },
		[&] (Region_map::Attach_error)      { return nullptr; }
	);

	log("validate pattern in sub rm");
	validate_pattern_at(test_pattern(), sub_rm_base + DS_SUB_OFFSET);

	/*
	 * Pre-populated sub rm session appeared correctly in the local address
	 * space. Now, test further populating the already attached sub rm session.
	 */

	log("attach RAM ds at another fixed position at sub rm");
	addr_t const DS_SUB_OFFSET_2 = 0x40000;
	sub_rm.attach(ds, {
		.size       = { },   .offset    = { },
		.use_at     = true,  .at        = DS_SUB_OFFSET_2,
		.executable = { },   .writeable = { }
	}).with_result(
		[&] (Region_map::Range const range) {
			if (range.start != DS_SUB_OFFSET_2)
				fail("attach-at return-value mismatch"); },
		[&] (Region_map::Attach_error) { }
	);

	log("validate pattern in second mapping in sub rm");
	validate_pattern_at(test_pattern(), sub_rm_base + DS_SUB_OFFSET_2);

	/*
	 * Try to cross the boundaries of the sub RM session. This should
	 * produce an error.
	 */
	sub_rm.attach(ds, {
		.size       = { },  .offset    = { },
		.use_at     = true, .at        = SUB_RM_SIZE - 4096,
		.executable = { },  .writeable = true
	}).with_result(
		[&] (Region_map::Range) {
			fail("undetected boundary conflict\n"); },
		[&] (Region_map::Attach_error e) {
			if (e == Region_map::Attach_error::REGION_CONFLICT)
				log("attaching beyond sub RM boundary failed as expected"); }
	);

	/*
	 * Check for working region - conflict detection
	 */
	log("attaching RAM ds to a conflicting region");
	sub_rm.attach(ds, {
		.size       = { },  .offset    = { },
		.use_at     = true, .at        = DS_SUB_OFFSET + 4096,
		.executable = { },  .writeable = true
	}).with_result(
		[&] (Region_map::Range) {
			fail("region conflict went undetected"); },
		[&] (Region_map::Attach_error e) {
			if (e == Region_map::Attach_error::REGION_CONFLICT)
				log("attaching conflicting region failed as expected"); }
	);

	if (config.xml().attribute_value("attach_twice_forbidden", false)) {
		/*
		 * Try to double-attach the same sub RM session. This should fail
		 */
		log("attach sub rm again at local address space");
		sub_rm.attach(ds, {
			.size       = { },  .offset    = { },
			.use_at     = { },  .at        = { },
			.executable = { },  .writeable = true
		}).with_result(
			[&] (Region_map::Range) {
				fail("double attachment of sub RM session went undetected"); },
			[&] (Region_map::Attach_error e) {
				if (e == Region_map::Attach_error::REGION_CONFLICT)
					log("doubly attaching sub RM session failed as expected"); }
		);
	}

	/*
	 * Try attaching RAM ds with offset (skipping the first page of
	 * the RAM ds). We expect the second test pattern at the beginning
	 * of the region. The region size should be automatically reduced by one
	 * page.
	 */
	log("attach RAM ds with offset");
	addr_t const DS_SUB_OFFSET_3 = 0x80000;
	sub_rm.attach(ds, {
		.size       = { },   .offset    = 4096,
		.use_at     = true,  .at        = DS_SUB_OFFSET_3,
		.executable = { },   .writeable = true
	});
	validate_pattern_at(test_pattern_2(), sub_rm_base + DS_SUB_OFFSET_3);

	/*
	 * Add the size parameter to the mix, attaching only a window of two pages
	 * starting with the second page.
	 */
	log("attach RAM ds with offset and size");
	addr_t const DS_SUB_OFFSET_4 = 0xc0000;
	sub_rm.attach(ds, {
		.size       = 2*4096,  .offset    = 4096,
		.use_at     = true,    .at        = DS_SUB_OFFSET_4,
		.executable = { },     .writeable = true
	});
	validate_pattern_at(test_pattern_2(), sub_rm_base + DS_SUB_OFFSET_4);

	/*
	 * Detach the first attachment (to be validated by the run script by
	 * inspecting '/proc/pid/maps' after running the test.
	 */
	sub_rm.detach(DS_SUB_OFFSET);

	log("--- end of sub-rm test ---");

	/*
	 * Do not return from main function to allow the run script to inspect the
	 * memory mappings after completing the test.
	 */
	sleep_forever();
	
}
