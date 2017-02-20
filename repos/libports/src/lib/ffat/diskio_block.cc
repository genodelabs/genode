/*
 * \brief   Low level disk I/O module using a Block session
 * \author  Christian Prochaska
 * \date    2011-05-30
 */

/*
 * Copyright (C) 2011-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <base/allocator_avl.h>
#include <base/log.h>
#include <block_session/connection.h>

/* Ffat includes */
extern "C" {
#include <ffat/diskio.h>
}

using namespace Genode;

static bool const verbose = false;

static Genode::Allocator_avl _block_alloc(Genode::env()->heap());
static Block::Connection *_block_connection;
static size_t _blk_size = 0;
static Block::sector_t _blk_cnt  = 0;
static Block::Session::Tx::Source *_source;


extern "C" DSTATUS disk_initialize (BYTE drv)
{
	static bool initialized = false;

	if (verbose)
		Genode::log("disk_initialize(drv=", drv, ") called.");

	if (drv != 0) {
		Genode::error("only one disk drive is supported at this time.");
		return STA_NOINIT;
	}

	if (initialized) {
		Genode::error("drv 0 has already been initialized.");
		return STA_NOINIT;
	}

	try {
		_block_connection = new (Genode::env()->heap()) Block::Connection(&_block_alloc);
	} catch(...) {
		Genode::error("could not open block connection");
		return STA_NOINIT;
	}

	_source = _block_connection->tx();

	Block::Session::Operations  ops;
	_block_connection->info(&_blk_cnt, &_blk_size, &ops);

	/* check for read- and write-capability */
	if (!ops.supported(Block::Packet_descriptor::READ)) {
		Genode::error("block device not readable!");
		destroy(env()->heap(), _block_connection);
		return STA_NOINIT;
	}
	if (!ops.supported(Block::Packet_descriptor::WRITE)) {
		Genode::warning("block device not writeable!");
	}

	if (verbose)
		Genode::log(__func__, ": We have ", _blk_cnt, " blocks with a "
		            "size of ", _blk_size, " bytes");

	initialized = true;

	return 0;
}


extern "C" DSTATUS disk_status (BYTE drv)
{
	if (drv != 0) {
		Genode::error("only one disk drive is supported at this time.");
		return STA_NODISK;
	}

	return 0;
}


extern "C" DRESULT disk_read(BYTE drv, BYTE *buff, DWORD sector, BYTE count)
{
	if (verbose)
		Genode::log(__func__, ": disk_read(drv=", drv, ", buff=", buff, ", "
		            "sector=", sector, ", count=", count, ") called.");

	if (drv != 0) {
		Genode::error("only one disk drive is supported at this time.");
		return RES_ERROR;
	}

	/* allocate packet-descriptor for reading */
	Block::Packet_descriptor p(_source->alloc_packet(_blk_size),
	                           Block::Packet_descriptor::READ, sector, count);
	_source->submit_packet(p);
	p = _source->get_acked_packet();

	/* check for success of operation */
	if (!p.succeeded()) {
		Genode::error("could not read block(s)");
		_source->release_packet(p);
		return RES_ERROR;
	}

	memcpy(buff, _source->packet_content(p), count * _blk_size);

	_source->release_packet(p);
	return RES_OK;
}


#if _READONLY == 0
extern "C" DRESULT disk_write(BYTE drv, const BYTE *buff, DWORD sector, BYTE count)
{
	if (verbose)
		Genode::log(__func__, ": disk_write(drv=", drv, ", buff=", buff, ", "
		            "sector=", sector, ", count=", count, ") called.");

	if (drv != 0) {
		Genode::error("only one disk drive is supported at this time.");
		return RES_ERROR;
	}

	/* allocate packet-descriptor for writing */
	Block::Packet_descriptor p(_source->alloc_packet(_blk_size),
	                           Block::Packet_descriptor::WRITE, sector, count);

	memcpy(_source->packet_content(p), buff, count * _blk_size);

	_source->submit_packet(p);
	p = _source->get_acked_packet();

	/* check for success of operation */
	if (!p.succeeded()) {
		Genode::error("could not write block(s)");
		_source->release_packet(p);
		return RES_ERROR;
	}

	_source->release_packet(p);
	return RES_OK;
}
#endif /* _READONLY */


extern "C" DRESULT disk_ioctl(BYTE drv, BYTE ctrl, void *buff)
{
	Genode::warning(__func__, "(drv=", drv, ", ctrl=", ctrl, ", buff=", buff, ") "
	                "called - not yet implemented.");
	return RES_OK;
}


extern "C" DWORD get_fattime(void)
{
	Genode::warning(__func__, "() called - not yet implemented.");
	return 0;
}

