/*
 * \brief  Back-end header
 * \author Sebastian Sumpf
 * \date   2011-05-30
 */

/*
 * Copyright (C) 2011-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _PART_BLK_H_
#define _PART_BLK_H_

#include <base/exception.h>
#include <base/stdint.h>

namespace Partition {

	enum {
		MAX_PARTITIONS  = 32, /* maximum supported paritions */
		MAX_PACKET_SIZE = 1024 * 1024
    };

	/**
	 * The partition type
	 */
	struct Partition
	{
		Genode::uint32_t _lba;     /* logical block address on device */
		Genode::uint32_t _sectors; /* number of sectors in patitions */

		Partition(Genode::uint32_t lba, Genode::uint32_t sectors)
		: _lba(lba), _sectors(sectors) { }

		/**
		 * Write/read blocks
		 *
		 * \param block_nr block number of partition to access
		 * \param count    number of blocks to read/write
		 * \param buf      buffer which containing the data to write or which is
		 *                 filled by reads
		 * \param write    true for a write operations, false for a read
		 * \throw Io_error
		 */
		void io(unsigned long block_nr, unsigned long count, void *buf, bool write = false);
	};

	/**
	 * Excpetions
	 */
	class Io_error : public Genode::Exception {};

	/**
	 * Initialize the back-end and parse partitions information
	 *
	 * \throw Io_error
	 */
	void init();

	/**
	 * Return partition information
	 *
	 * \param num partition number
	 * \return    pointer to partition if it could be found, zero otherwise
	 */
	Partition  *partition(int num);

	/**
	 * Returns block size of back end
	 */
	Genode::size_t blk_size();

	/**
	 * Synchronize with backend device
	 */
	void sync();
}

#endif /* _PART_BLK_H_ */
