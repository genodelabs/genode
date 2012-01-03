/*
 * \brief  Block driver interface test
 * \author Sebastian Sumpf
 * \date   2011-08-11
 *
 * Test block device, read blocks add one to the data, write block back, read
 * block again and compare outputs
 */

/*
 * Copyright (C) 2011-2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#include <base/allocator_avl.h>
#include <base/printf.h>
#include <base/sleep.h>
#include <base/thread.h>
#include <block_session/connection.h>
#include <util/string.h>


static const bool read_only = false;

class Worker : public Genode::Thread<8192>
{
	private:

		Block::Connection _blk_con;
		Genode::size_t    _blk_size;

	public:

		/**
		 * Constructor
		 */
		Worker(Genode::Allocator_avl *block_alloc)
		: _blk_con(block_alloc) { }

		void dump(Block::Packet_descriptor &p1, Block::Packet_descriptor &p2)
		{
			Block::Session::Tx::Source *source   = _blk_con.tx();
			unsigned *d1 = (unsigned *)source->packet_content(p1);
			unsigned *d2 = (unsigned *)source->packet_content(p2);
			for (int i = 0; i < 128; i += 8) {
				Genode::printf("1 0x%02x: %08x %08x %08x %08x %08x %08x %08x %08x\n", i,
				               d1[i], d1[i+1], d1[i+2], d1[i+3], d1[i+4], d1[i+5], d1[i+6], d1[i+7]);
				Genode::printf("2 0x%02x: %08x %08x %08x %08x %08x %08x %08x %08x\n\n", i,
				               d2[i], d2[i+1], d2[i+2], d2[i+3], d2[i+4], d2[i+5], d2[i+6], d2[i+7]);
			}

		}

		void compare(Genode::size_t block, Block::Packet_descriptor &p1, Block::Packet_descriptor &p2)
		{
			using namespace Genode;
			Block::Session::Tx::Source *source   = _blk_con.tx();
			char *d1 = source->packet_content(p1);
			char *d2 = source->packet_content(p2);

			bool equal = true;
			for (size_t i = 0; i < _blk_size / sizeof(unsigned); i++)
				if (d1[i] != d2[i]) {
					equal = false;

					if (!read_only)
						PERR("%zu: %x != %x", i, d1[i], d2[i]);
				}

			printf("Comparing block %010zu: ", block);
			if (equal)
				printf("success\n");
			else {
				printf("failed\n");
				dump(p1, p2);
			}
		}

		void modify(Block::Packet_descriptor &src, Block::Packet_descriptor &dst, int val)
		{
			Block::Session::Tx::Source *source   = _blk_con.tx();
			for (unsigned j = 0; j < _blk_size; j++)
				source->packet_content(dst)[j] = source->packet_content(src)[j] + val;
		}

		void submit(Block::Packet_descriptor &src,
		            Block::Packet_descriptor &dst,
		            int val, Genode::size_t block, bool cmp)
		{
			Block::Session::Tx::Source *source   = _blk_con.tx();

			source->submit_packet(src);
			src = source->get_acked_packet();

			/* check for success of operation */
			if (!src.succeeded()) {
				PWRN("Could not read block %zu", block);
				return;
			}

			if (cmp)
				compare(block, src, dst);

			modify(src, dst, val);

			if (read_only)
				return;

			source->submit_packet(dst);
			dst = source->get_acked_packet();

			/* check for success of operation */
			if (!dst.succeeded())
				PWRN("Could not write block %zu", block);
		}

		/**
		 * Thread's entry function.
		 */
		void entry()
		{
			using namespace Genode;

			Block::Session::Tx::Source *source   = _blk_con.tx();
			size_t                      blk_cnt  = 0;
			Block::Session::Operations  ops;
			_blk_con.info(&blk_cnt, &_blk_size, &ops);

			/* check for read- and write-capability */
			if (!ops.supported(Block::Packet_descriptor::READ)) {
				PERR("Block device not readable!");
				return;
			}
			if (!ops.supported(Block::Packet_descriptor::WRITE)) {
				PERR("Block device not writeable!");
				return;
			}

			printf("We have %zu blocks with a size of %zu bytes (%zu MB)\n",
			       blk_cnt, _blk_size, blk_cnt / (2 * 1024));

			/* now, repeatedly invert each single block of the device */
			size_t step = blk_cnt / 32;
			for (size_t i = 0; i < blk_cnt; i += step) {
				try {
					/* allocate packet-descriptor for reading */
					Block::Packet_descriptor p(source->alloc_packet(_blk_size),
					                           Block::Packet_descriptor::READ, i);

					/* allocate a packet-descriptor for writing */
					Block::Packet_descriptor q(source->alloc_packet(_blk_size),
					                           Block::Packet_descriptor::WRITE, i);

					submit(p, q,  1, i, false);
					submit(p, q, -1, i, true);

					/* release packets */
					source->release_packet(q);
					source->release_packet(p);
				} catch (Block::Session::Tx::Source::Packet_alloc_failed) {
					PWRN("Mmh, strange we run out of packets");
					source->release_packet(source->get_acked_packet());
				}
			}

			env()->parent()->close(_blk_con.cap());
			env()->parent()->exit(0);
		}
};


int main(int argc, char **argv)
{
	Genode::printf("--- AHCI block driver test ---\n");

	Genode::Allocator_avl block_alloc(Genode::env()->heap());
	Worker th(&block_alloc);
	th.start();
	Genode::sleep_forever();
	return 0;
}
