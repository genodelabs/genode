/**
 * \brief  Connect rump kernel to Genode's block interface
 * \author Sebastian Sumpf
 * \date   2013-12-16
 */

/*
 * Copyright (C) 2013-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include "sched.h"
#include <base/allocator_avl.h>
#include <base/printf.h>
#include <block_session/connection.h>
#include <rump/env.h>
#include <rump_fs/fs.h>


static const bool verbose = false;

enum  { GENODE_FD = 64 };

/**
 * Block session connection
 */
class Backend
{
	private:

		Genode::Allocator_avl              _alloc { &Rump::env().heap() };
		Block::Connection                  _session { Rump::env().env(), &_alloc };
		Genode::size_t                     _blk_size; /* block size of the device   */
		Block::sector_t                    _blk_cnt;  /* number of blocks of device */
		Block::Session::Operations         _blk_ops;
		Genode::Lock                       _session_lock;

	public:

		Backend()
		{
			_session.info(&_blk_cnt, &_blk_size, &_blk_ops);
		}

		uint64_t block_count() const { return (uint64_t)_blk_cnt; }
		size_t   block_size()  const { return (size_t)_blk_size; }

		bool writable()
		{
			return _blk_ops.supported(Block::Packet_descriptor::WRITE);
		}

		void sync()
		{
			Genode::Lock::Guard guard(_session_lock);
			_session.sync();
		}

		bool submit(int op, int64_t offset, size_t length, void *data)
		{
			using namespace Block;

			Genode::Lock::Guard guard(_session_lock);

			Packet_descriptor::Opcode opcode;
			opcode = op & RUMPUSER_BIO_WRITE ? Packet_descriptor::WRITE :
			                                   Packet_descriptor::READ;
			/* allocate packet */
			try {
				Packet_descriptor packet( _session.dma_alloc_packet(length),
				                         opcode, offset / _blk_size,
				                         length / _blk_size);

				/* out packet -> copy data */
				if (opcode == Packet_descriptor::WRITE)
					Genode::memcpy(_session.tx()->packet_content(packet), data, length);

				_session.tx()->submit_packet(packet);
			} catch(Block::Session::Tx::Source::Packet_alloc_failed) {
				Genode::error("I/O back end: Packet allocation failed!");
				return false;
			}

			/* wait and process result */
			Packet_descriptor packet = _session.tx()->get_acked_packet();

			/* in packet */
			if (opcode == Packet_descriptor::READ)
				Genode::memcpy(data, _session.tx()->packet_content(packet), length);

			bool succeeded = packet.succeeded();
			_session.tx()->release_packet(packet);

			/* sync request */
			if (op & RUMPUSER_BIO_SYNC) {
				_session.sync();
			}

			return succeeded;
		}
};


static Backend &backend()
{
	static Backend _b;
	return _b;
}


int rumpuser_getfileinfo(const char *name, uint64_t *size, int *type)
{
	if (Genode::strcmp(GENODE_BLOCK_SESSION, name))
		return ENXIO;

	if (type)
		*type = RUMPUSER_FT_BLK;

	if (size)
		*size = backend().block_count() * backend().block_size();

	return 0;
}


int rumpuser_open(const char *name, int mode, int *fdp)
{
	if (!(mode & RUMPUSER_OPEN_BIO || Genode::strcmp(GENODE_BLOCK_SESSION, name)))
		return ENXIO;

	/* check for writable */
	if ((mode & RUMPUSER_OPEN_ACCMODE) && !backend().writable())
		return EROFS;

	*fdp = GENODE_FD;
	return 0;
}


void rumpuser_bio(int fd, int op, void *data, size_t dlen, int64_t off, 
                  rump_biodone_fn biodone, void *donearg)
{
	int nlocks;
	rumpkern_unsched(&nlocks, 0);

	/* data request */
	if (verbose)
		Genode::log("fd: ",   fd,   " "
		            "op: ",   op,   " "
		            "len: ",  dlen, " "
		            "off: ",  Genode::Hex((unsigned long)off), " "
		            "bio ",   donearg, " "
		            "sync: ", !!(op & RUMPUSER_BIO_SYNC));

	bool succeeded = backend().submit(op, off, dlen, data);

	rumpkern_sched(nlocks, 0);

	if (biodone)
		biodone(donearg, dlen, succeeded ? 0 : EIO);
}


void rump_io_backend_sync()
{
	backend().sync();
}


void rump_io_backend_init()
{
	/* create back end */
	backend();
}


void rumpuser_dprintf(const char *fmt, ...)
{
	va_list list;
	va_start(list, fmt);

	Genode::vprintf(fmt, list);

	va_end(list);
}
