/**
 * \brief  Connect rump kernel Genode's block interface
 * \author Sebastian Sumpf
 * \date   2013-12-16
 */

/*
 * Copyright (C) 2013-2014 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#include "sched.h"
#include <base/allocator_avl.h>
#include <base/thread.h>
#include <base/printf.h>
#include <block_session/connection.h>
#include <rump_fs/fs.h>
#include <util/list.h>
#include <util/string.h>


static const bool verbose = false;

enum  { GENODE_FD = 64 };

struct Packet : Genode::List<Packet>::Element
{
	Block::sector_t                  blk;
	Genode::size_t                   cnt;
	Block::Packet_descriptor::Opcode opcode;
	void                            *data;
	long                             offset;
	rump_biodone_fn                  biodone;
	void                            *donearg;
	bool                             valid;
	bool                             pending;
	bool                             sync;
};


class Backend : public Hard_context_thread
{

	private:

		enum { COUNT = Block::Session::TX_QUEUE_SIZE };

		Genode::Allocator_avl              _alloc;
		Block::Connection                  _session;
		Genode::size_t                     _blk_size; /* block size of the device   */
		Block::sector_t                    _blk_cnt;  /* number of blocks of device */
		Block::Session::Operations         _blk_ops;
		Packet                             _p[COUNT];
		Genode::Semaphore                  _alloc_sem;
		Genode::Semaphore                  _packet_sem;
		int                                _index_thread = 0;
		int                                _index_client = 0;
		Genode::Lock                       _alloc_lock;
		bool                               _handle;
		Genode::Signal_receiver            _receiver;
		Genode::Signal_dispatcher<Backend> _disp_ack;
		Genode::Signal_dispatcher<Backend> _disp_submit;


		Genode::List<Packet> *_pending()
		{
			static Genode::List<Packet> _p;
			return &_p;
		}

		bool _have_pending()
		{
			return !!_pending()->first();
		}

		Packet *_find(Block::Packet_descriptor &packet)
		{
			Packet *p = _pending()->first();
			while (p) {
				if (p->offset == packet.offset())
					return p;

				p = p->next();
			}

			Genode::error("pending packet not found");
			return 0;
		}
		
		Packet *_dequeue()
		{
			int idx;
			for (int i = 0; i < COUNT; i++) {
				idx = (_index_thread + i) % COUNT;
				if (_p[idx].valid && !_p[idx].pending) {
					_index_thread   = idx;
					_p[idx].pending = true;
					return &_p[idx];
				}
			}
			Genode::warning("dequeue returned 0");
			return 0;
		}


		void _free(Packet *p)
		{
			p->valid   = false;
			p->pending = false;
			_alloc_sem.up();
		}

		void _ready_to_submit(unsigned) { _handle = false; }

		void _ack_avail(unsigned)
		{
			_handle = false;

			while (_session.tx()->ack_avail()) {
				Block::Packet_descriptor packet = _session.tx()->get_acked_packet();
				Packet *p = _find(packet);

				if (p->opcode == Block::Packet_descriptor::READ)
					Genode::memcpy(p->data, _session.tx()->packet_content(packet),
					               packet.block_count() * _blk_size);


				/* sync session if requested  */
				if (p->sync)
					_session.sync();

				int dummy;
				if (verbose)
					Genode::log("BIO done  p: ", p, " bio ", p->donearg);

				rumpkern_sched(0, 0);
				if (p->biodone)
					p->biodone(p->donearg, p->cnt * _blk_size,
					           packet.succeeded() ? 0 : EIO);
				rumpkern_unsched(&dummy, 0);

				_session.tx()->release_packet(packet);
				_pending()->remove(p);
				_free(p);
			}
		}

		void _handle_signal()
		{
			_handle = true;

			while (_handle) {
				Genode::Signal s = _receiver.wait_for_signal();
				static_cast<Genode::Signal_dispatcher_base *>
					(s.context())->dispatch(s.num());
			}
		}

	protected:

		void entry()
		{
			_rump_upcalls.hyp_schedule();
			_rump_upcalls.hyp_lwproc_newlwp(0);
			_rump_upcalls.hyp_unschedule();

			while (true) {

				while (_packet_sem.cnt() <= 0 && _have_pending())
					_handle_signal();

				_packet_sem.down();

				while (!_session.tx()->ready_to_submit())
					_handle_signal();

				Packet *p = _dequeue();

				/* zero or sync request */
				if (!p->cnt) {

					if (p->sync)
						_session.sync();

					_free(p);
					continue;
				}

				for (bool done = false; !done;)
					try {
						Block::Packet_descriptor packet(
						_session.dma_alloc_packet(p->cnt * _blk_size),
						                          p->opcode, p->blk, p->cnt);
						/* got packet copy data */
						if (p->opcode == Block::Packet_descriptor::WRITE)
							Genode::memcpy(_session.tx()->packet_content(packet),
							               p->data, p->cnt * _blk_size);
						_session.tx()->submit_packet(packet);

						/* mark as pending */
						p->offset = packet.offset();
						_pending()->insert(p);
						done = true;
					} catch(Block::Session::Tx::Source::Packet_alloc_failed) {
					_handle_signal();
				}
			}
		}

	public:

		Backend()
		: Hard_context_thread("block_io", 0, 0, 0, false),
		_alloc(Genode::env()->heap()),
		_session(&_alloc),
		_alloc_sem(COUNT),
		_disp_ack(_receiver, *this, &Backend::_ack_avail),
		_disp_submit(_receiver, *this, &Backend::_ready_to_submit)
		{
			_session.tx_channel()->sigh_ack_avail(_disp_ack);
			_session.tx_channel()->sigh_ready_to_submit(_disp_submit);
			_session.info(&_blk_cnt, &_blk_size, &_blk_ops);
			Genode::log("Backend blk_size ", _blk_size);
			Genode::memset(_p, 0, sizeof(_p));
			start();
		}

		uint64_t block_count() const { return (uint64_t)_blk_cnt; }
		size_t   block_size()  const { return (size_t)_blk_size; }

		bool writable()
		{
			return _blk_ops.supported(Block::Packet_descriptor::WRITE);
		}

		Packet *alloc()
		{
			int idx;
			_alloc_sem.down();
			Genode::Lock::Guard guard(_alloc_lock);
			for (int i = 0; i < COUNT; i++) {
				idx = (_index_client + i) % COUNT;
				if (!_p[idx].valid) {
					_p[idx].valid   = true;
					_p[idx].pending = false;
					_index_client = idx;
					return &_p[idx];
				}
			}
			Genode::warning("alloc returned 0");
			return 0;
		}

		void submit() { _packet_sem.up(); }
};


static Backend *backend()
{
	static Backend *_b = 0;

	if (_b)
		return _b;

	int nlocks;
	rumpkern_unsched(&nlocks, 0);
	try {
		_b = new(Genode::env()->heap())Backend();
	} catch (Genode::Parent::Service_denied) {
		Genode::error("opening block session denied!");
	}
	rumpkern_sched(nlocks, 0);

	return _b;
}


int rumpuser_getfileinfo(const char *name, uint64_t *size, int *type)
{
	if (Genode::strcmp(GENODE_BLOCK_SESSION, name))
		return ENXIO;

	if (type)
		*type = RUMPUSER_FT_BLK;

	if (size)
		*size = backend()->block_count() * backend()->block_size();

	return 0;
}


int rumpuser_open(const char *name, int mode, int *fdp)
{
	if (!(mode & RUMPUSER_OPEN_BIO || Genode::strcmp(GENODE_BLOCK_SESSION, name)))
		return ENXIO;

	/* check for writable */
	if ((mode & RUMPUSER_OPEN_ACCMODE) && !backend()->writable())
		return EROFS;

	*fdp = GENODE_FD;
	return 0;
}


void rumpuser_bio(int fd, int op, void *data, size_t dlen, int64_t off, 
                  rump_biodone_fn biodone, void *donearg)
{
	int nlocks;

	rumpkern_unsched(&nlocks, 0);
	Packet *p = backend()->alloc();

	if (verbose)
		Genode::log("fd: ",   fd,   " "
		            "op: ",   op,   " "
		            "len: ",  dlen, " "
		            "off: ",  Genode::Hex((unsigned long)off), " "
		            "p: ",    p,       " "
		            "bio ",   donearg, " "
		            "sync: ", !!(op & RUMPUSER_BIO_SYNC));

	p->opcode= op & RUMPUSER_BIO_WRITE ? Block::Packet_descriptor::WRITE :
	                                     Block::Packet_descriptor::READ;

	p->cnt     = dlen / backend()->block_size();
	p->blk     = off  / backend()->block_size();
	p->data    = data;
	p->biodone = biodone;
	p->donearg = donearg;
	p->sync    = !!(op & RUMPUSER_BIO_SYNC);
	backend()->submit();
	rumpkern_sched(nlocks, 0);
}


void rump_io_backend_sync()
{
	/* send empty packet with sync request */
	Packet *p = backend()->alloc();
	p->cnt    = 0;
	p->sync   = true;
	backend()->submit();
}


void rumpuser_dprintf(const char *fmt, ...)
{
	va_list list;
	va_start(list, fmt);

	Genode::vprintf(fmt, list);

	va_end(list);
}
