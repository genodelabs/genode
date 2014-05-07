/*
 * \brief  SATA benchmark for Exynos5 platform
 * \author Martin Stein
 * \date   2012-06-05
 */

/*
 * Copyright (C) 2012-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <base/sleep.h>
#include <base/printf.h>
#include <timer_session/connection.h>
#include <os/attached_ram_dataspace.h>
#include <dataspace/client.h>

/* local includes */
#include <ahci_driver.h>


struct Operation
{
	virtual void operator () (Block::Driver &driver,
	                          Genode::addr_t block_number,
	                          Genode::size_t block_count,
	                          Genode::addr_t buffer_phys,
	                          char          *buffer_virt) = 0;
};

void print_bench_head()
{
	Genode::printf("\n");
	Genode::printf("bytes/block       bytes    sec          MB/sec\n");
	Genode::printf("----------------------------------------------\n");
}

/*
 * \param total_size    total number of bytes to read
 * \param request_size  number of bytes per request
 */
static void run_benchmark(Block::Driver  &driver,
                          Timer::Session &timer,
                          char           *buffer_virt,
                          Genode::addr_t  buffer_phys,
                          Genode::size_t  buffer_size,
                          Genode::size_t  request_size,
                          Operation      &operation)
{
	using namespace Genode;
	if (request_size > buffer_size) {
		PERR("undersized buffer %u, need %u", buffer_size, buffer_size);
		while (1) ;
	}
	size_t const block_count = request_size / driver.block_size();
	/*
	 * The goal is to get 5 test repetitions that took 2 s < time < 2.3 s,
	 * each. Thus we start with count = 32 and then adjust the count
	 * for a retry if the test time is not in range.
	 */
	unsigned tmp_bytes  = 64 * request_size;;
	unsigned bytes      = 0;
	unsigned ms         = 0;
	unsigned reps       = 0;
	float    sec        = 0;
	float    mb_per_sec = 0;
	while (1)
	{
		size_t num_requests = tmp_bytes / request_size;

		/* do measurement */
		unsigned const time_before_ms = timer.elapsed_ms();
		for (unsigned i = 0; i < num_requests; i++)
		{
			addr_t const block_number = i * block_count;
			operation(driver, block_number, block_count,
			          buffer_phys, buffer_virt);
		}
		/* read results */
		unsigned const time_after_ms = timer.elapsed_ms();
		ms = time_after_ms - time_before_ms;

		/* check if test time in range */
		if (ms < 2000 || ms >= 2300) {
			/*
			 * adjust transfer amount according to measured time
			 *
			 * FIXME implement static inertia
			 */
			tmp_bytes = (((float) 2150 / ms) * tmp_bytes);
			tmp_bytes &= 0xfffffe00; /* align to 512 byte blocks */
		} else {
			/* if new result is better than the last one do update */
			float const tmp_mb         = ((float)tmp_bytes / 1000) / 1000;
			float const tmp_sec        = (float)ms / 1000;
			float const tmp_mb_per_sec = (float)tmp_mb / tmp_sec;
			if (tmp_mb_per_sec > mb_per_sec) {
				sec        = tmp_sec;
				mb_per_sec = tmp_mb_per_sec;
				bytes      = tmp_bytes;
			}
			/* check if we need more repetitions */
			reps++;
			if (reps == 5) break;
		}
	}
	unsigned const sec_left         = sec;
	unsigned const sec_right        = 1000 * ((float)sec - sec_left);
	unsigned const mb_per_sec_left  = mb_per_sec;
	unsigned const mb_per_sec_right = 1000 * ((float)mb_per_sec - mb_per_sec_left);
	PLOG(" %10u  %10u  %u.%03u  %10u.%03u", request_size, bytes, sec_left, sec_right, mb_per_sec_left, mb_per_sec_right);
}


int main(int argc, char **argv)
{
	using namespace Genode;

	printf("AHCI bench\n");
	printf("==========\n");
	printf("\n");

	static Ahci_driver driver;

	static Timer::Connection timer;

	long const request_sizes[] = {
		1048576, 262144, 16384, 8192, 4096, 2048, 1024, 512, 0 };

	/* total size of communication buffer */
	size_t const buffer_size = 1024*1024;

	/* allocate read/write buffer */
	static Attached_ram_dataspace buffer(env()->ram_session(), buffer_size, false);
	char * const buffer_virt = buffer.local_addr<char>();
	addr_t const buffer_phys = Dataspace_client(buffer.cap()).phys_addr();

	/*
	 * Benchmark reading from SATA device
	 */

	printf("read\n");
	printf("~~~~\n");
	print_bench_head();

	struct Read : Operation
	{
		void operator () (Block::Driver &driver,
		                  addr_t number, size_t count, addr_t phys, char *virt)
		{
			Block::Packet_descriptor packet;
			if (driver.dma_enabled())
				driver.read_dma(number, count, phys, packet);
			else
				driver.read(number, count, virt, packet);
		}
	} read_operation;

	for (unsigned i = 0; request_sizes[i]; i++)
		run_benchmark(driver, timer, buffer_virt, buffer_phys, buffer_size,
		              request_sizes[i], read_operation);

	/*
	 * Benchmark writing to SATA device
	 *
	 * Attention: Original data will be overridden on target drive
	 */

	printf("\n");
	printf("write\n");
	printf("~~~~~\n");
	print_bench_head();

	struct Write : Operation
	{
		void operator () (Block::Driver &driver,
		                  addr_t number, size_t count, addr_t phys, char *virt)
		{
			Block::Packet_descriptor packet;
			if (driver.dma_enabled())
				driver.write_dma(number, count, phys, packet);
			else
				driver.write(number, count, virt, packet);
		}
	} write_operation;

	for (unsigned i = 0; request_sizes[i]; i++)
		run_benchmark(driver, timer, buffer_virt, buffer_phys, buffer_size,
		              request_sizes[i], write_operation);

	printf("\n");
	printf("benchmark finished\n");
	sleep_forever();
	return 0;
}
