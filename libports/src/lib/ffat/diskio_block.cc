/*
 * \brief   Low level disk I/O module using a Block session
 * \author  Christian Prochaska
 * \date    2011-05-30
 */

/*
 * Copyright (C) 2011-2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <base/allocator_avl.h>
#include <base/printf.h>
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
static size_t _blk_cnt  = 0;
static Block::Session::Tx::Source *_source;


extern "C" DSTATUS disk_initialize (BYTE drv)
{
	static bool initialized = false;

	if (verbose)
		PDBG("disk_initialize(drv=%u) called.", drv);

	if (drv != 0) {
		PERR("Only one disk drive is supported at this time.");
		return STA_NOINIT;
	}

	if (initialized) {
		PERR("drv 0 has already been initialized.");
		return STA_NOINIT;
	}

	try {
		_block_connection = new (Genode::env()->heap()) Block::Connection(&_block_alloc);
	} catch(...) {
		PERR("could not open block connection");
		return STA_NOINIT;
	}

	_source = _block_connection->tx();

	Block::Session::Operations  ops;
	_block_connection->info(&_blk_cnt, &_blk_size, &ops);

	/* check for read- and write-capability */
	if (!ops.supported(Block::Packet_descriptor::READ)) {
		PERR("Block device not readable!");
		destroy(env()->heap(), _block_connection);
		return STA_NOINIT;
	}
	if (!ops.supported(Block::Packet_descriptor::WRITE)) {
		PINF("Block device not writeable!");
	}

	if (verbose)
		PDBG("We have %zu blocks with a size of %zu bytes",
		     _blk_cnt, _blk_size);

	initialized = true;

	return 0;
}


extern "C" DSTATUS disk_status (BYTE drv)
{
	if (drv != 0) {
		PERR("Only one disk drive is supported at this time.");
		return STA_NODISK;
	}

	return 0;
}


extern "C" DRESULT disk_read(BYTE drv, BYTE *buff, DWORD sector, BYTE count)
{
	if (verbose)
		PDBG("disk_read(drv=%u, buff=%p, sector=%u, count=%u) called.",
		     drv, buff, (unsigned int)sector, count);

	if (drv != 0) {
		PERR("Only one disk drive is supported at this time.");
		return RES_ERROR;
	}

	/* allocate packet-descriptor for reading */
	Block::Packet_descriptor p(_source->alloc_packet(_blk_size),
	                           Block::Packet_descriptor::READ, sector, count);
	_source->submit_packet(p);
	p = _source->get_acked_packet();

	/* check for success of operation */
	if (!p.succeeded()) {
		PERR("Could not read block(s)");
		_source->release_packet(p);
		return RES_ERROR;
	}

	memcpy(buff, _source->packet_content(p), count * _blk_size);

#if 0
	PDBG("read() successful, contents of buff = \n");

	for (unsigned int i = 0; i < count * _blk_size; i++)
		PDBG("%8x: %2x %c", i, buff[i], buff[i] >= 32 ? buff[i] : '-');
#endif

	_source->release_packet(p);
	return RES_OK;
}


#if _READONLY == 0
extern "C" DRESULT disk_write(BYTE drv, const BYTE *buff, DWORD sector, BYTE count)
{
	if (verbose)
		PDBG("disk_write(drv=%u, buff=%p, sector=%u, count=%u) called.",
		     drv, buff, (unsigned int)sector, count);

	if (drv != 0) {
		PERR("Only one disk drive is supported at this time.");
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
		PERR("Could not write block(s)");
		_source->release_packet(p);
		return RES_ERROR;
	}

	_source->release_packet(p);
	return RES_OK;
}
#endif /* _READONLY */


extern "C" DRESULT disk_ioctl(BYTE drv, BYTE ctrl, void *buff)
{
	PWRN("disk_ioctl(drv=%u, ctrl=%u, buff=%p) called - not yet implemented.",
	     drv, ctrl, buff);
	return RES_OK;
}


extern "C" DWORD get_fattime(void)
{
	PWRN("get_fattime() called - not yet implemented.");
	return 0;
}

