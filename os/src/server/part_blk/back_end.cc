/*
 * \brief  Back end to other block interface
 * \author Sebsastian Sumpf
 * \date   2011-05-24
 */

/*
 * Copyright (C) 2011-2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */
#include <base/allocator_avl.h>
#include <base/semaphore.h>
#include <block_session/connection.h>
#include "part_blk.h"

using namespace Genode;

/**
 * Used to block until a packet has been freed
 */
static Semaphore _alloc_sem(0);

namespace Partition {

	size_t _blk_cnt;
	size_t _blk_size;

	Allocator_avl        _block_alloc(env()->heap());
	Block::Connection    _blk(&_block_alloc, 4 * MAX_PACKET_SIZE);

	Partition           *_part_list[MAX_PARTITIONS]; /* contains pointers to valid partittions or 0 */

	Partition           *partition(int num) { return (num < MAX_PARTITIONS) ? _part_list[num] : 0; }
	size_t               blk_size()         { return  _blk_size; }
	inline unsigned long max_packets()      { return (MAX_PACKET_SIZE / _blk_size); }


	/**
	 * Partition table entry format
	 */
	struct Partition_record
	{
		enum { INVALID = 0, EXTENTED = 0x5 };
		uint8_t  _unused[4];
		uint8_t  _type;       /* partition type */
		uint8_t  _unused2[3];
		uint32_t _lba;        /* logical block address */
		uint32_t _sectors;    /* number of sectors */

		bool is_valid()    { return _type != INVALID; }
		bool is_extented() { return _type == EXTENTED; }
	} __attribute__((packed));


	/**
	 * Master/Extented boot record format
	 */
	struct Mbr
	{
		uint8_t                   _unused[446];
		Partition_record          _records[4];
		uint16_t                  _magic;

		bool is_valid()
		{
			/* magic number of partition table */
			enum { MAGIC = 0xaa55 };
			return _magic == MAGIC;
		}
	} __attribute__((packed));


	class Sector
	{
		private:

			Block::Packet_descriptor _p;

		protected:

			static Lock _lock;

		public:

			Sector(unsigned long blk_nr, unsigned long count, bool write = false)
			{
				using namespace Block;
				Lock::Guard guard(_lock);
				Block::Packet_descriptor::Opcode op = write ? Block::Packet_descriptor::WRITE
				                                            : Block::Packet_descriptor::READ;
					_p = Block::Packet_descriptor(_blk.dma_alloc_packet(_blk_size * count),
					                              op,  blk_nr, count);
			}

			void submit_request()
			{
				Lock::Guard guard(_lock);
				_blk.tx()->submit_packet(_p);
				_p = _blk.tx()->get_acked_packet();

				/* unblock clients that possibly wait for packet stream allocations */
				if (_alloc_sem.cnt() < 0)
					_alloc_sem.up();

				if (!_p.succeeded()) {
					PERR("Could not access block %zu", _p.block_number());
					throw Io_error();
				}
			}

			~Sector()
			{
				Lock::Guard guard(_lock);
				_blk.tx()->release_packet(_p);
			}

			template <typename T>
			T addr() { return reinterpret_cast<T>(_blk.tx()->packet_content(_p)); }
	};

	Lock Sector::_lock;


	void parse_extented(Partition_record *record)
	{
		Partition_record *r = record;
		unsigned lba = r->_lba;

		/* first logical partition number */
		int nr = 5;
		do {
			Sector s(lba, 1);
			s.submit_request();
			Mbr *ebr = s.addr<Mbr *>();

			if (!(ebr->is_valid()))
				throw Io_error();

			/* The first record is the actual logical partition. The lba of this
			 * partition is relative to the lba of the current EBR */
			Partition_record *logical = &(ebr->_records[0]);
			if (logical->is_valid() && nr < MAX_PARTITIONS) {
				_part_list[nr++] = new (env()->heap()) Partition(logical->_lba + lba, logical->_sectors);

				PINF("Partition %d: LBA %u (%u blocks) type %x", nr - 1,
				     logical->_lba + lba, logical->_sectors, logical->_type);
			}

			/* the second record points to the next EBR (relative form this EBR)*/
			r = &(ebr->_records[1]);
			lba += ebr->_records[1]._lba;

		} while (r->is_valid());
	}


	void parse_mbr(Mbr *mbr)
	{
		/* no partition table, use whole disc as partition 0 */
		if (!(mbr->is_valid()))
			_part_list[0] = new(env()->heap())Partition(0, _blk_cnt - 1);

		for (int i = 0; i < 4; i++) {
			Partition_record *r = &(mbr->_records[i]);

			if (r->is_valid())
				PINF("Partition %d: LBA %u (%u blocks) type: %x", i + 1, r->_lba,
				     r->_sectors, r->_type);
			else
				continue;

			if (r->is_extented())
				parse_extented(r);
			else
				_part_list[i + 1] = new(env()->heap()) Partition(r->_lba, r->_sectors);
		}
	}


	void init()
	{
		Block::Session::Operations ops;

		/* device info */
		_blk.info(&_blk_cnt, &_blk_size, &ops);

		/* read MBR */
		Sector s(0, 1);
		s.submit_request();
		parse_mbr(s.addr<Mbr *>());
	}


	void _io(unsigned long lba, unsigned long count, uint8_t *buf, bool write)
	{
		unsigned long bytes;

		while (count) {

			unsigned long curr_count = min<unsigned long>(count, max_packets());
			bytes                    = curr_count * _blk_size;
			bool alloc = false;
			while (!alloc) {
				try {
					Sector sec(lba, curr_count, write);
		
					if (!write) {
						sec.submit_request();
						memcpy(buf, sec.addr<void *>(), bytes);
					}
					else {
						memcpy(sec.addr<void *>(), buf, bytes);
						sec.submit_request();
					}

					alloc = true;
				} catch (Block::Session::Tx::Source::Packet_alloc_failed) {
					/* block */
					_alloc_sem.down();
				}
			}

			lba   += curr_count;
			count -= curr_count;
			buf   += bytes;
		}

		/* zero out rest of page */
		bytes = (count * _blk_size) % 4096;
		if (bytes)
			memset(buf, 0, bytes);
	}


	void Partition::io(unsigned long block_nr, unsigned long count, void *buf, bool write)
	{
		if (block_nr + count > _sectors)
			throw Io_error();

		_io(_lba + block_nr, count, (uint8_t *)buf, write);
	}
}
