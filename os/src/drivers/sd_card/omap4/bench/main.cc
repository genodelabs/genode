/*
 * \brief  SD-card benchmark OMAP4 platform
 * \author Norman Feske
 * \date   2012-07-19
 */

/* Genode includes */
#include <base/sleep.h>
#include <base/printf.h>
#include <timer_session/connection.h>
#include <os/attached_ram_dataspace.h>

/* local includes */
#include <driver.h>


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

	PLOG("request_size=%zd bytes", request_size);

	size_t const time_before_ms = timer.elapsed_ms();

	size_t num_requests = buffer_size / request_size;

	for (size_t i = 0; i < num_requests; i++)
	{
		size_t const block_count  = request_size / driver.block_size();
		addr_t const block_number = i*block_count;

		operation(driver, block_number, block_count,
		          buffer_phys + i*request_size,
		          buffer_virt + i*request_size);
	}

	size_t const time_after_ms = timer.elapsed_ms();
	size_t const duration_ms   = time_after_ms - time_before_ms;

	/*
	 * Convert bytes per milliseconds to kilobytes per seconds
	 *
	 * (total_size / 1024) / (duration_ms / 1000)
	 */
	size_t const buffer_size_kb = buffer_size / 1024;
	size_t const throughput_kb_per_sec = (1000*buffer_size_kb) / duration_ms;

	PLOG("      -> duration:   %zd ms", duration_ms);
	PLOG("         throughput: %zd KiB/sec", throughput_kb_per_sec);
}


int main(int argc, char **argv)
{
	using namespace Genode;

	printf("--- OMAP4 SD card benchmark ---\n");

	bool const use_dma = false;

	static Block::Omap4_driver driver(use_dma);

	static Timer::Connection timer;

	long const request_sizes[] = {
		512, 1024, 2048, 4096, 8192, 16384, 32768, 64*1024, 128*1024, 0 };

	/* total size of communication buffer */
	size_t const buffer_size = 10*1024*1024;

	/* allocate read/write buffer */
	static Attached_ram_dataspace buffer(env()->ram_session(), buffer_size, false);
	char * const buffer_virt = buffer.local_addr<char>();
	addr_t const buffer_phys = Dataspace_client(buffer.cap()).phys_addr();

	/*
	 * Benchmark reading from SD card
	 */

	printf("\n-- reading from SD card --\n");

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
	 * Benchmark writing to SD card
	 *
	 * We write back the content of the buffer, which we just filled during the
	 * read benchmark. If both read and write succeed, the SD card will retain
	 * its original content.
	 */

	printf("\n-- writing to SD card --\n");

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

	printf("\n--- OMAP4 SD card benchmark finished ---\n");
	sleep_forever();
	return 0;
}
