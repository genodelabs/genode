/*
 * \brief  SD-card benchmark
 * \author Norman Feske
 * \author Martin Stein
 * \date   2012-07-19
 */

/*
 * Copyright (C) 2012-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <timer_session/connection.h>
#include <base/attached_ram_dataspace.h>
#include <base/attached_rom_dataspace.h>
#include <platform_session/dma_buffer.h>

/* local includes */
#include <driver.h>

using namespace Genode;


struct Benchmark
{
	using Packet_descriptor = Block::Packet_descriptor;

	struct Block_operation_failed : Exception { };

	enum Operation { READ, WRITE };

	struct Driver_session : Block::Driver_session_base
	{
		Signal_transmitter sig;
		unsigned long      nr_of_acks { 0 };

		Driver_session(Signal_context_capability sig) : sig(sig) { }

		void ack_packet(Packet_descriptor &, bool success) override
		{
			if (!success) {
				throw Block_operation_failed(); }
			nr_of_acks++;
			sig.submit();
		}
	};

	Env                      &env;
	Platform::Connection      platform     { env };
	Attached_rom_dataspace    config       { env, "config" };
	Packet_descriptor         pkt          { };
	uint64_t                  time_before_ms { };
	Timer::Connection         timer        { env };
	Operation                 operation    { READ };
	Signal_handler<Benchmark> ack_handler  { env.ep(), *this, &Benchmark::update_state };
	Driver_session            drv_session  { ack_handler };
	Sd_card::Driver           drv          { env, platform };
	size_t const              buf_size_kib { config.xml().attribute_value("buffer_size_kib",
	                                                                      (size_t)0) };
	size_t const              buf_size     { buf_size_kib * 1024 };
	Platform::Dma_buffer      buf          { platform, buf_size, UNCACHED };
	char                     *buf_virt     { buf.local_addr<char>() };
	size_t                    buf_off_done { 0 };
	size_t                    buf_off_pend { 0 };
	unsigned                  req_size_id  { 0 };
	size_t                    req_sizes[9] { 512, 1024, 1024 * 2,  1024 * 4,
	                                         1024 * 8,  1024 * 16, 1024 * 32,
	                                         1024 * 64, 1024 * 128 };

	size_t req_size() const { return req_sizes[req_size_id]; }

	void update_state()
	{
		/* raise done counter and check if the buffer is full */
		buf_off_done += drv_session.nr_of_acks * req_size();
		drv_session.nr_of_acks = 0;
		if (buf_off_done == buf_size) {

			/* print stats for the current request size */
			uint64_t const time_after_ms = timer.elapsed_ms();
			uint64_t const duration_ms   = time_after_ms - time_before_ms;
			size_t   const kib_per_sec   = (size_t)((1000 * buf_size_kib)
			                                        / duration_ms);
			log("      duration:   ", duration_ms,  " ms");
			log("      amount:     ", buf_size_kib, " KiB");
			log("      throughput: ", kib_per_sec,  " KiB/sec");

			/* go to next request size */
			buf_off_pend = 0;
			buf_off_done = 0;
			req_size_id++;

			/* check if we have done all request sizes for an operation */
			if (req_size_id == sizeof(req_sizes)/sizeof(req_sizes[0])) {

				/* go to next operation or end the test */
				log("");
				req_size_id = 0;
				switch (operation) {
				case READ:
					operation = WRITE;
					log("-- writing to SD card --");
					break;
				case WRITE:
					log("--- SD card benchmark finished ---");
					return;
				}
			}
			log("   request size ", req_size(), " bytes");
			time_before_ms = timer.elapsed_ms();
		}
		/* issue as many requests for the current request size as possible */
		try {
			size_t const block_size = drv.info().block_size;
			size_t const cnt = req_size() / block_size;
			for (; buf_off_pend < buf_size; buf_off_pend += req_size()) {

				/* calculate block offset */
				addr_t const nr  = buf_off_pend / block_size;

				if (drv.dma_enabled()) {

					/* request with DMA */
					addr_t const phys = buf.dma_addr() + buf_off_pend;
					switch (operation) {
					case READ:   drv.read_dma(nr, cnt, phys, pkt); break;
					case WRITE: drv.write_dma(nr, cnt, phys, pkt); break; }

				} else {

					/* request without DMA */
					char *const virt = buf_virt + buf_off_pend;
					switch (operation) {
					case READ:   drv.read(nr, cnt, virt, pkt); break;
					case WRITE: drv.write(nr, cnt, virt, pkt); break; }
				}
			}
		} catch (Block::Driver::Request_congestion) { }
	}

	Benchmark(Env &env) : env(env)
	{
		log("");
		log("--- SD card benchmark (", drv.dma_enabled() ? "with" : "no", " DMA) ---");

		drv.session(&drv_session);

		/* start issuing requests */
		log("");
		log("-- reading from SD card --");
		log("   request size ", req_size(), " bytes");
		time_before_ms = timer.elapsed_ms();
		update_state();
	}

	private:

		/*
		 * Noncopyable
		 */
		Benchmark(Benchmark const &);
		Benchmark &operator = (Benchmark const &);
};
