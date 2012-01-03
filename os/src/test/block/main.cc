/*
 * \brief  Block session test implementation
 * \author Stefan Kalkowski
 * \date   2010-07-07
 *
 * The test program inverts the bits block-by-block of a block-device.
 */

/*
 * Copyright (C) 2010-2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#include <base/allocator_avl.h>
#include <base/printf.h>
#include <base/sleep.h>
#include <base/thread.h>
#include <block_session/connection.h>

class Inverter : public Genode::Thread<8192>
{
	private:

		Block::Connection _blk_con;

	public:

		/**
		 * Constructor
		 */
		Inverter(Genode::Allocator_avl *block_alloc)
		: _blk_con(block_alloc) { }

		/**
		 * Thread's entry function.
		 */
		void entry()
		{
			using namespace Genode;

			Block::Session::Tx::Source *source   = _blk_con.tx();
			size_t                      blk_size = 0;
			size_t                      blk_cnt  = 0;
			Block::Session::Operations  ops;
			_blk_con.info(&blk_cnt, &blk_size, &ops);

			/* check for read- and write-capability */
			if (!ops.supported(Block::Packet_descriptor::READ)) {
				PERR("Block device not readable!");
				return;
			}
			if (!ops.supported(Block::Packet_descriptor::WRITE)) {
				PERR("Block device not writeable!");
				return;
			}

			PLOG("We have %zu blocks with a size of %zu bytes",
			     blk_cnt, blk_size);

			/* now, repeatedly invert each single block of the device */
			for (unsigned round = 1; ; ++round) {
				PLOG("ROUND %d", round);
				for (size_t i = 0; i < blk_cnt; i++) {
					try {
						/* allocate packet-descriptor for reading */
						Block::Packet_descriptor p(source->alloc_packet(blk_size),
						                           Block::Packet_descriptor::READ, i);
						source->submit_packet(p);
						p = source->get_acked_packet();

						/* check for success of operation */
						if (!p.succeeded()) {
							PWRN("Could not read block %zu", i);
							continue;
						}

						/* allocate a packet-descriptor for writing */
						Block::Packet_descriptor q(source->alloc_packet(blk_size),
						                           Block::Packet_descriptor::WRITE, i);

						/*
						 * Copy inverted bytes of the read-block
						 * into the packet payload.
						 */
						for (unsigned j = 0; j < blk_size; j++)
							source->packet_content(q)[j] =
								~source->packet_content(p)[j];
						source->submit_packet(q);
						q = source->get_acked_packet();

						/* check for success of operation */
						if (!q.succeeded())
							PWRN("Could not write block %zu", i);

						/* release packets */
						source->release_packet(p);
						source->release_packet(q);
					} catch (Block::Session::Tx::Source::Packet_alloc_failed) {
						PWRN("Mmh, strange we run out of packets");
						source->release_packet(source->get_acked_packet());
					}
				}
			}
		}
};


int main(int argc, char **argv)
{
	Genode::printf("--- Block session test ---\n");

	Genode::Allocator_avl block_alloc(Genode::env()->heap());
	Inverter th(&block_alloc);
	th.start();
	Genode::sleep_forever();
	return 0;
}
