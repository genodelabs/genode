/*
 * \brief  Test for accessing memory using the monitor runtime
 * \author Norman Feske
 * \date   2023-06-06
 *
 * This test exercises the memory-access functionality of the monitor component
 * by acting as both the monitored inferior and the observer at the same time.
 */

/*
 * Copyright (C) 2023 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <base/component.h>
#include <base/sleep.h>
#include <base/attached_ram_dataspace.h>
#include <rm_session/connection.h>
#include <timer_session/connection.h>
#include <region_map/client.h>
#include <monitor_controller.h>

namespace Test {

	using namespace Genode;

	struct Main;
}


struct Test::Main
{
	Env &_env;

	Monitor::Controller _monitor { _env };

	static void assert(bool condition, auto &&... error_message)
	{
		if (!condition) {
			error(error_message...);
			sleep_forever();
		}
	}

	void test_query_threads()
	{
		log("-- test_query_threads --");
		using Thread_info = Monitor::Controller::Thread_info;

		bool expected_inferior_detected = false;
		_monitor.for_each_thread([&] (Thread_info const &info) {
			if (info.name == "test-monitor" && info.pid == 1 && info.tid == 1)
				expected_inferior_detected = true;
			log("thread inferior:", info.pid, " tid:", info.tid, " name:", info.name);
		});
		assert(expected_inferior_detected, "failed to detect myself as inferior");
	}

	void test_survive_nonexisting_memory_access()
	{
		log("-- test_survive_nonexisting_memory_access --");
		char buffer[32] { };
		size_t const read_bytes =
			_monitor.read_memory(0x10, Byte_range_ptr(buffer, sizeof(buffer)));

		assert(read_bytes == 0,
		       "unexpected read of ", read_bytes, " from nonexisting memory");
	}

	void test_read_memory()
	{
		log("-- test_read_memory --");
		char const * const s = "Trying to read back this pattern";
		size_t const num_bytes = strlen(s);
		char buffer[num_bytes] { };

		size_t const read_bytes =
			_monitor.read_memory((addr_t)s, Byte_range_ptr(buffer, sizeof(buffer)));

		assert(read_bytes == num_bytes,
		       "unable to read string of ", num_bytes, " bytes");
		assert(equal(Const_byte_range_ptr(buffer, read_bytes), s),
		       "read bytes don't match expected pattern");
	}

	void test_truncated_mapping()
	{
		log("-- test_truncated_mapping --");

		/*
		 * Attach 4 KiB of RAM at at the beginning of a managed dataspace of 8
		 * KiB while leaving second 4 KiB unmapped.
		 */

		Rm_connection rm_connection { _env };
		Region_map_client rm { rm_connection.create(8*1024) };
		Attached_ram_dataspace ram_ds { _env.ram(), _env.rm(), 4*1024 };
		rm.attach_at(ram_ds.cap(), 0);
		Attached_dataspace managed_ds { _env.rm(), rm.dataspace() };

		/* try to read 100 bytes at page boundary, expect to stop after 50 bytes */
		char buffer[100] { };
		addr_t const at = (addr_t)managed_ds.local_addr<char>()
		                + 4*1024 - 50;
		size_t const read_bytes =
			_monitor.read_memory(at, Byte_range_ptr(buffer, sizeof(buffer)));

		assert(read_bytes == 50, "failed to read from truncated mapping");
	}

	void test_bench()
	{
		log("-- test_bench --");

		Timer::Connection timer { _env };

		Attached_ram_dataspace large_ram_ds { _env.ram(), _env.rm(), 8*1024*1024 };

		memset(large_ram_ds.local_addr<char>(), 1, large_ram_ds.size());

		/*
		 * Dimensioning of the buffer for one round trip:
		 *
		 * The terminal_crosslink component uses a buffer of 4 KiB and
		 * the debug monitor limits the 'm' command response to 2 KiB to leave
		 * enough space for asynchronous notifications and protocol overhead.
		 * GDB's 'm' command encodes memory as hex, two characters per byte.
		 * Hence, a dump of max. 1 KiB is currently possible.
		 *
		 * The most effective way to optimize the throughput would be to
		 * increase the terminal-crosslink's buffer size, reducing the number
		 * of round trips.
		 */
		char buffer[1024] { };

		uint64_t const start_us = timer.elapsed_us();
		uint64_t       now_us   = start_us;

		uint64_t total_bytes = 0;
		addr_t offset = 0;
		for (;;) {

			addr_t const at = (addr_t)large_ram_ds.local_addr<char>() + offset;

			size_t const read_bytes =
				_monitor.read_memory(at, Byte_range_ptr(buffer, sizeof(buffer)));

			assert(read_bytes == sizeof(buffer),
			       "failed to read memory during benchmark");

			total_bytes += read_bytes;

			/* slide read window over large dataspace, wrap at the end */
			offset = offset + sizeof(buffer);
			if (offset + sizeof(buffer) >= large_ram_ds.size())
				offset = 0;

			now_us = timer.elapsed_us();
			if (now_us - start_us > 3*1024*1024)
				break;
		}

		double const seconds = double(now_us - start_us)/double(1000*1000);
		double const rate_kib = (double(total_bytes)/1024) / seconds;
		log("read ", total_bytes, " bytes at rate of ", rate_kib, " KiB/s");
	}

	void test_writeable_text_segment()
	{
		log("-- test_writeable_text_segment --");

		/*
		 * Test the 'wx' attribute of the <monitor> <policy>, that converts
		 * executable text segments into writeable RAM.
		 */

		char *code_ptr = (char *)&Component::construct;
		char const * const pattern = "risky";

		copy_cstring(code_ptr, pattern, strlen(pattern) + 1);

		assert(strcmp(code_ptr, pattern, strlen(pattern)) == 0,
		       "unexpected content at patched address");
	}

	void test_write_memory()
	{
		log("-- test_write_memory --");
		char const * const s     = "Trying to modify this pattern   ";
		char const * const s_new = "Trying to read back this pattern";
		size_t const num_bytes = strlen(s);
		char buffer[num_bytes] { };

		assert(_monitor.write_memory((addr_t)s,
		                             Const_byte_range_ptr(s_new, num_bytes)),
		       "unable to write string of ", num_bytes, " bytes");

		size_t const read_bytes =
			_monitor.read_memory((addr_t)s, Byte_range_ptr(buffer, sizeof(buffer)));

		assert(read_bytes == num_bytes,
		       "unable to read string of ", num_bytes, " bytes");
		assert(equal(Const_byte_range_ptr(buffer, read_bytes), s_new),
		       "read bytes don't match expected pattern");
	}

	Main(Env &env) : _env(env)
	{
		test_query_threads();
		test_survive_nonexisting_memory_access();
		test_read_memory();
		test_truncated_mapping();
		test_writeable_text_segment();
		test_write_memory();
		test_bench();

		_env.parent().exit(0);
	}
};


void Component::construct(Genode::Env &env) { static Test::Main main(env); }
