/*
 * \brief  Block driver interface test
 * \author Sebastian Sumpf
 * \date   2011-08-11
 *
 * Test block device, read blocks add one to the data, write block back, read
 * block again and compare outputs
 */

/*
 * Copyright (C) 2011-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <base/allocator_avl.h>
#include <base/printf.h>
#include <base/sleep.h>
#include <block_session/connection.h>
#include <util/string.h>
#include <timer_session/connection.h>

using namespace Genode;

enum { VERBOSE = 0 };

/*
 * Do a bench for a specific access command and request size
 *
 * \param timer         timer for measurements
 * \param request_size  number of bytes per request
 * \param source        block session packet sink
 * \param block_size    block size of the targeted device
 * \param max_lba       maximum logical block address of the device
 */
static void run_benchmark(
                          Timer::Session &             timer,
                          Genode::size_t               request_size,
                          Block::Session::Tx::Source * source,
                          size_t                       block_size,
                          unsigned                     max_lba,
                          void *                       buf,
                          bool                         write)
{
	/*
	 * request_size: how many bytes are accessed by one command
	 * block_size:   raw block size that is used by the drive (not the software)
	 * block_count:  how many raw blocks are accessed by one command
	 */
	size_t const block_count = request_size / block_size;
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
		if ((num_requests * block_count) >= max_lba) { PERR("X"); while(1); }

		/* do measurement */
		unsigned const time_before_ms = timer.elapsed_ms();
		for (unsigned i = 0; i < num_requests; i++)
		{
			/* create packet */
			addr_t const lba = i * block_count;
			Block::Packet_descriptor p(
				source->alloc_packet(request_size),
				write ? Block::Packet_descriptor::WRITE :
				        Block::Packet_descriptor::READ,
				lba, block_count);

			/* simulate payload */
			void * pptr = source->packet_content(p);
			if (write) memcpy(pptr, buf, request_size);

			/* submit packet */
			source->submit_packet(p);
			p = source->get_acked_packet();
			if (!p.succeeded()) {
				PERR("could not access block %lu", lba);
				while (1) ;
			}
			/* simulate payload */
			if (!write) memcpy(buf, pptr, request_size);

			/* release packet */
			source->release_packet(p);
		}
		/* read results */
		unsigned const time_after_ms = timer.elapsed_ms();
		ms = time_after_ms - time_before_ms;

		/* trial log */
		if (VERBOSE)
			printf("%s %u bytes in %u ms\n",
			       write ? "written" : "read", tmp_bytes, ms);

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

void print_bench_head()
{
	printf("\n");
	printf("bytes/block       bytes    sec          MB/sec\n");
	printf("----------------------------------------------\n");
}

int main(int argc, char **argv)
{
	using namespace Genode;

	printf("AHCI bench\n");
	printf("==========\n");

	enum { TX_BUF_SIZE   = 2 * 1024 * 1024 };

	/* get block connection */
	Genode::Allocator_avl        block_alloc(Genode::env()->heap());
	Block::Connection            _blk_con(&block_alloc, TX_BUF_SIZE);
	Block::Session::Tx::Source * source = _blk_con.tx();

	/* check block-connection info */
	Genode::size_t             _blk_size = 0;
	Block::sector_t              blk_cnt = 0;
	Block::Session::Operations ops;
	_blk_con.info(&blk_cnt, &_blk_size, &ops);
	if (!ops.supported(Block::Packet_descriptor::READ)) {
		PERR("Block device not readable!");
		while(1); }
	if (!ops.supported(Block::Packet_descriptor::WRITE)) {
		PERR("Block device not writeable!");
		while(1); }

	/* data for payload simulation */
	enum { BUF_SIZE = 1 * 1024 * 1024 };
	void * buf = env()->heap()->alloc(BUF_SIZE);
	for (unsigned o = 0; o < BUF_SIZE; o += sizeof(unsigned))
		*(unsigned volatile *)((addr_t)buf + o) = 0x12345678;

	static Timer::Connection timer;

	long const request_sizes[] = {
		1048576, 262144, 16384, 8192, 4096, 2048, 1024, 512, 0 };

	/*
	 * Benchmark reading from SATA device
	 */

	printf("\n");
	printf("read\n");
	printf("~~~~\n");
	print_bench_head();

	for (unsigned i = 0; request_sizes[i]; i++)
		run_benchmark(timer, request_sizes[i], source, _blk_size, blk_cnt, buf, 0);

	/*
	 * Benchmark writing to SATA device
	 *
	 * Attention: Original data will be overridden on target drive
	 */

	printf("\n");
	printf("write\n");
	printf("~~~~~\n");
	print_bench_head();

	for (unsigned i = 0; request_sizes[i]; i++)
		run_benchmark(timer, request_sizes[i], source, _blk_size, blk_cnt, buf, 1);

	printf("\n");
	printf("benchmark finished\n");
	sleep_forever();
	return 0;
}

