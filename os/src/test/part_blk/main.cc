/*
 * \brief  Test that reads and writes the first and the last block of a given
 *         block device
 * \author Sebastian Sumpf
 * \date   2011-05-30
 */

/*
 * Copyright (C) 2011-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#include <base/allocator_avl.h>
#include <block_session/connection.h>
#include <os/config.h>

static const int verbose = 0;
using namespace Genode;

static Allocator_avl        _block_alloc(env()->heap());
static Block::Connection    _blk(&_block_alloc);
static size_t _blk_size;


class Sector
{
	private:

		Block::Packet_descriptor _p;

	public:

		Sector(unsigned long blk_nr, unsigned long count, bool write = false)
		{
			Block::Packet_descriptor::Opcode op = write ? Block::Packet_descriptor::WRITE
			                                            : Block::Packet_descriptor::READ;
			try {
				_p = Block::Packet_descriptor( _blk.dma_alloc_packet(_blk_size * count),
				                              op, blk_nr, count);
			} catch (Block::Session::Tx::Source::Packet_alloc_failed) {
				PERR("Packet overrun!");
				_p = _blk.tx()->get_acked_packet();
			}
		}

		~Sector() { _blk.tx()->release_packet(_p); }

		template <typename T>
		T addr() { return reinterpret_cast<T>(_blk.tx()->packet_content(_p)); }

		void submit_request()
		{
			_blk.tx()->submit_packet(_p);
			_p = _blk.tx()->get_acked_packet();

			if (!_p.succeeded())
				PERR("Could not access block %llu", _p.block_number());
		}

};


int main()
{
	unsigned pattern;
	try {
		Genode::config()->xml_node().attribute("pattern").value(&pattern); }
	catch (...) { PERR("Test Failed"); return 1; }

	size_t blk_count;
	Block::Session::Operations ops;
	_blk.info(&blk_count, &_blk_size, &ops);

	if (verbose)
		printf("Found device %zu blocks of %zu bytes\n", blk_count, _blk_size);

	/* write first and last block of device useing 'pattern' */
	{
		Sector s(0, 1, true);
		memset(s.addr<void *>(), pattern, _blk_size);
		s.submit_request();

		Sector s_last(blk_count - 1, 1, true);
		memset(s_last.addr<void *>(), 2 * pattern, _blk_size);
		s_last.submit_request();
	}

	/* read first and last block from device and compare to 'pattern' */
	Sector s(0, 1);
	s.submit_request();

	Sector s_last(blk_count - 1, 1);
	s_last.submit_request();

	unsigned *val      = s.addr<unsigned *>();
	unsigned *val_last = s_last.addr<unsigned *>();

	unsigned cmp_val, cmp_val_last;
	memset(&cmp_val, pattern, 4);
	memset(&cmp_val_last, 2 * pattern, 4);

	if (verbose) {
		printf("READ blk %05u: %x\n", 0, *val);
		printf("READ blk %05zu: %x\n", blk_count - 1, *val_last);
	}

	if (*val != cmp_val || *val_last != cmp_val_last)
		printf("Failed\n");
	else
		printf("Success\n");

	return 0;
}
