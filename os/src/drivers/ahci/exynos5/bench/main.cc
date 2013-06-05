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

	/*
	 * The goal is to get a test that took 2 s < time < 2.3 s,
	 * thus we start with count = 32 and then adjust the count
	 * for a retry if the test time is not in range.
	 */
	unsigned bytes = 64 * request_size;
	unsigned ms    = 0;
	while (1)
	{
		/* calculate test parameters */
		if (bytes > buffer_size) {
			PERR("undersized buffer %u, need %u", buffer_size, bytes);
			while (1) ;
		}
		size_t num_requests = bytes / request_size;

		/* do measurement */
		size_t const time_before_ms = timer.elapsed_ms();
		for (size_t i = 0; i < num_requests; i++)
		{
			size_t const block_count  = request_size / driver.block_size();
			addr_t const block_number = i*block_count;

			operation(driver, block_number, block_count,
					  buffer_phys + i*request_size,
					  buffer_virt + i*request_size);
		}
		/* read results */
		size_t const time_after_ms = timer.elapsed_ms();
		ms = time_after_ms - time_before_ms;

		/*
		 * leave or adjust transfer amount according to measured time
		 *
		 * FIXME implement static inertia
		 */
		if (ms < 2000 || ms >= 2300) {
			bytes = (((float) 2150 / ms) * bytes);
			bytes &= 0xfffffe00; /* align to 512 byte blocks */
			printf("retry with %u B\n", bytes);
		} else break;
	}
	/* convert and print results */
	float const    mb       = ((float)bytes / 1000) / 1000;
	unsigned const mb_left  = mb;
	unsigned const mb_right = 1000 * ((float)mb - mb_left);
	float const    sec       = (float)ms / 1000;
	unsigned const sec_left  = sec;
	unsigned const sec_right = 1000 * ((float)sec - sec_left);
	float const    mb_per_sec       = (float)mb / sec;
	unsigned const mb_per_sec_left  = mb_per_sec;
	unsigned const mb_per_sec_right = 1000 * ((float)mb_per_sec
	                                          - mb_per_sec_left);
	PLOG(" %10u  %10u  %10u.%03u  %u.%03u  %10u.%03u", request_size, bytes,
	     mb_left, mb_right, sec_left, sec_right, mb_per_sec_left,
	     mb_per_sec_right);
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
	size_t const buffer_size = 600*1024*1024;

	/* allocate read/write buffer */
	static Attached_ram_dataspace buffer(env()->ram_session(), buffer_size,
	                                     false);
	char * const buffer_virt = buffer.local_addr<char>();
	addr_t const buffer_phys = Dataspace_client(buffer.cap()).phys_addr();

	/*
	 * Benchmark reading from SATA device
	 */

	printf("read\n");
	printf("~~~~\n");
	printf("\n");
	printf("bytes/block       bytes              MB    sec          MB/sec\n");
	printf("--------------------------------------------------------------\n");

	struct Read : Operation
	{
		void operator () (Block::Driver &driver,
		                  addr_t number, size_t count, addr_t phys, char *virt)
		{
			if (driver.dma_enabled())
				driver.read_dma(number, count, phys);
			else
				driver.read(number, count, virt);
		}
	} read_operation;

	for (unsigned i = 0; request_sizes[i]; i++)
		run_benchmark(driver, timer, buffer_virt, buffer_phys, buffer_size,
		              request_sizes[i], read_operation);

	/*
	 * Benchmark writing to SATA device
	 *
	 * We write back the content of the buffer, which we just filled during the
	 * read benchmark. If both read and write succeed, the SATA device
	 * will retain its original content.
	 */

	printf("\n");
	printf("write\n");
	printf("~~~~~\n");
	printf("\n");
	printf("bytes/block       bytes              MB    sec          MB/sec\n");
	printf("--------------------------------------------------------------\n");

	struct Write : Operation
	{
		void operator () (Block::Driver &driver,
		                  addr_t number, size_t count, addr_t phys, char *virt)
		{
			if (driver.dma_enabled())
				driver.write_dma(number, count, phys);
			else
				driver.write(number, count, virt);
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
