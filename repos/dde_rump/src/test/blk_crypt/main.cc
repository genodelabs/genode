/**
 * \brief  Block encryption test
 * \author Josef Soentgen
 * \date   2014-04-16
 */

#include <base/allocator_avl.h>
#include <block_session/connection.h>

int main(int argc, char *argv[])
{
	try {
		char buffer[512];
		Block::Session::Operations blk_ops;
		Genode::size_t             blk_sz;
		Block::sector_t            blk_cnt;

		Genode::Allocator_avl alloc(Genode::env()->heap());
		Block::Connection blk(&alloc);
		blk.info(&blk_cnt, &blk_sz, &blk_ops);

		Genode::log("block device with block size ", blk_sz, " sector count ", blk_cnt);

		Genode::log("read first block");

		Block::Packet_descriptor p(blk.tx()->alloc_packet(blk_sz),
		                           Block::Packet_descriptor::READ, 0, 1);
		blk.tx()->submit_packet(p);
		p = blk.tx()->get_acked_packet();

		if (!p.succeeded()) {
			Genode::error("could not read first block");
			blk.tx()->release_packet(p);
			return 1;
		}

		Genode::memcpy(buffer, blk.tx()->packet_content(p), blk_sz);

		/* XXX compare content */
	} catch(Genode::Parent::Service_denied) {
		Genode::error("opening block session was denied");
		return -1;
	}

	return 0;
}
