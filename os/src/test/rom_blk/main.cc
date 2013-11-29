/*
 * \brief  Rom-file to block-session client test implementation
 * \author Stefan Kalkowski
 * \date   2010-07-07
 *
 * The test program compares the values delivered by the block-service,
 * with the original rom-file.
 */

/*
 * Copyright (C) 2010-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#include <base/allocator_avl.h>
#include <base/printf.h>
#include <base/sleep.h>
#include <base/exception.h>
#include <base/thread.h>
#include <os/config.h>
#include <block_session/connection.h>
#include <rom_session/connection.h>


class Comparer : public Genode::Thread<8192>
{
	private:

		Block::Connection          _blk_con;
		Genode::Rom_connection     _rom;
		Genode::addr_t             _addr;

		class Block_file_differ : Genode::Exception {};

	public:

		enum {
			BLOCK_REQ_PARALLEL = 10 /* number of blocks to handle per block-request */
		};

		Comparer(Genode::Allocator_avl *block_alloc,
		         const char* filename)
		: Thread("comparer"), _blk_con(block_alloc), _rom(filename),
		  _addr(Genode::env()->rm_session()->attach(_rom.dataspace())) { }

		void entry()
		{
			using namespace Genode;

			Block::Session::Tx::Source *source   = _blk_con.tx();
			size_t                      blk_size = 0;
			size_t                      blk_cnt  = 0;
			Genode::addr_t              end      =
				_addr + Dataspace_client(_rom.dataspace()).size();
			Block::Session::Operations  ops;
			_blk_con.info(&blk_cnt, &blk_size, &ops);

			if (!ops.supported(Block::Packet_descriptor::READ)) {
				PERR("Block device not readable!");
			}

			PINF("We have %zx blocks with a size of %zx bytes", blk_cnt, blk_size);

			for (size_t i = 0; i < blk_cnt; i += BLOCK_REQ_PARALLEL) {
				try {
					size_t cnt = (blk_cnt - i > BLOCK_REQ_PARALLEL)
						? BLOCK_REQ_PARALLEL : blk_cnt - i;
					Block::Packet_descriptor p(source->alloc_packet(cnt * blk_size),
					                           Block::Packet_descriptor::READ, i, cnt);

					source->submit_packet(p);
					p = source->get_acked_packet();

					if (!p.succeeded()) {
						PERR("Could not read block %zx-%zx", i, i+cnt);
						return;
					}

					char* blk_src = source->packet_content(p);
					char* rom_src = (char*) _addr + i * blk_size;
					bool differ = false;
					for (size_t j = 0; j < cnt; j++)
						for (size_t k = 0; k < blk_size; k++) {
							if (&rom_src[j*blk_size+k] >= (char*)end) {
								PERR("End of image file reached!");
								return;
							}
							if (blk_src[j*blk_size+k] != rom_src[j*blk_size+k])
								differ = true;
						}
					if (differ) {
						PWRN("block %zx differs!", i);
						throw Block_file_differ();
					}
					source->release_packet(p);
				} catch (Block::Session::Tx::Source::Packet_alloc_failed) {
					PERR("Mmh, strange we run out of packets");
					return;
				}
			}
			PINF("all done, finished!");
		}
};


int main(int argc, char **argv)
{
	using namespace Genode;

	PINF("--- Block session test ---\n");

	try {
		static char filename[64];
		config()->xml_node().attribute("file").value(filename, sizeof(filename));
		Allocator_avl block_alloc(env()->heap());
		Comparer th(&block_alloc, filename);
		th.start();
		sleep_forever();
	} catch (Rom_connection::Rom_connection_failed) {
		PERR("Config file or file given by <filename> tag is missing.");
	}
	PINF("An error occured, exit now ...");
	return -1;
}
