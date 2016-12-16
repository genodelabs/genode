/*
 * \brief  SD-card benchmark
 * \author Norman Feske
 * \author Martin Stein
 * \date   2012-07-19
 */

/*
 * Copyright (C) 2012-2016 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <base/component.h>
#include <base/log.h>
#include <timer_session/connection.h>
#include <os/attached_ram_dataspace.h>
#include <os/config.h>

/* local includes */
#include <driver.h>

using namespace Genode;

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

	log("request_size=", request_size, " bytes");

	size_t const time_before_ms = timer.elapsed_ms();

	size_t num_requests = buffer_size / request_size;

	/*
	 * Trim number of requests if it would take to much time
	 */
	if (num_requests > 320) {
		buffer_size = 320 * request_size;
		num_requests = buffer_size / request_size;
	}

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

	log("         duration:   ", duration_ms,           " ms");
	log("         amount:     ", buffer_size_kb,        " KiB");
	log("         throughput: ", throughput_kb_per_sec, " KiB/sec");
}


struct Main
{
	Main(Env &env)
	{
		log("--- SD card benchmark ---");


		static Block::Sdhci_driver driver(env);
		bool const use_dma = driver.dma_enabled();

		static Timer::Connection timer;

		long const request_sizes[] = {
			512, 1024, 2048, 4096, 8192, 16384, 32768, 64*1024, 128*1024, 0 };

		/* total size of communication buffer */
		size_t const buffer_size = 10 * 1024 * 1024;

		/* allocate read/write buffer */
		static Attached_ram_dataspace buffer(&env.ram(), buffer_size, Genode::UNCACHED);
		char * const buffer_virt = buffer.local_addr<char>();
		addr_t const buffer_phys = Dataspace_client(buffer.cap()).phys_addr();

		/*
		 * Benchmark reading from SD card
		 */

		log("\n-- reading from SD card (", use_dma ? "" : "not ", "using DMA) --");

		struct Read : Operation
		{
			void operator () (Block::Driver &driver,
							  addr_t number, size_t count, addr_t phys, char *virt)
			{
				Block::Packet_descriptor p;
				if (driver.dma_enabled())
					driver.read_dma(number, count, phys, p);
				else
					driver.read(number, count, virt, p);
			}
		} read_operation;

		for (unsigned i = 0; request_sizes[i]; i++) {
			run_benchmark(driver, timer, buffer_virt, buffer_phys, buffer_size,
						  request_sizes[i], read_operation);
		}

		/*
		 * Benchmark writing to SD card
		 *
		 * We write back the content of the buffer, which we just filled during the
		 * read benchmark. If both read and write succeed, the SD card will retain
		 * its original content.
		 */

		log("\n-- writing to SD card (", use_dma ? "" : "not ", "using DMA) --");

		struct Write : Operation
		{
			void operator () (Block::Driver &driver,
			                  addr_t number, size_t count, addr_t phys, char *virt)
			{
				Block::Packet_descriptor p;
				if (driver.dma_enabled())
					driver.write_dma(number, count, phys, p);
				else
					driver.write(number, count, virt, p);
			}
		} write_operation;

		for (unsigned i = 0; request_sizes[i]; i++)
			run_benchmark(driver, timer, buffer_virt, buffer_phys, buffer_size,
			              request_sizes[i], write_operation);

		log("\n--- SD card benchmark finished ---");
	}
};


void Component::construct(Genode::Env &env) { static Main main(env); }
