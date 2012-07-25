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


/*
 * \param total_size    total number of bytes to read
 * \param request_size  number of bytes per request
 */
static void test_read(Block::Driver  &driver,
                      Timer::Session &timer,
                      char           *buffer_virt,
                      Genode::addr_t  buffer_phys,
                      Genode::size_t  buffer_size,
                      Genode::size_t  request_size)
{
	using namespace Genode;

	PLOG("read: request_size=%zd bytes", request_size);

	size_t const time_before_ms = timer.elapsed_ms();

	size_t num_requests = buffer_size / request_size;

	for (size_t i = 0; i < num_requests; i++)
	{
		size_t const block_count  = request_size / driver.block_size();
		addr_t const block_number = i*block_count;

		if (driver.dma_enabled()) {
			driver.read_dma(block_number, block_count,
			                buffer_phys + i*request_size);
		} else {
			driver.read(block_number, block_count,
			            buffer_virt + i*request_size);
		}
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

	bool const use_dma = true;

	static Block::Omap4_driver driver(use_dma);

	static Timer::Connection timer;

	long const request_sizes[] = {
		512, 1024, 2048, 4096, 8192, 16384, 32768, 0 };

	/* total size of communication buffer */
	size_t const buffer_size = 10*1024*1024;

	/* allocate read buffer */
	static Attached_ram_dataspace buffer(env()->ram_session(), buffer_size, false);

	char * const buffer_virt = buffer.local_addr<char>();
	addr_t const buffer_phys = Dataspace_client(buffer.cap()).phys_addr();

	for (unsigned i = 0; request_sizes[i]; i++)
		test_read(driver, timer, buffer_virt, buffer_phys, buffer_size,
		          request_sizes[i]);

	printf("--- OMAP4 SD card benchmark finished ---\n");
	sleep_forever();
	return 0;
}
