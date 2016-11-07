/*
 * \brief  Block-session driver for partition server
 * \author Stefan Kalkowski
 * \date   2013-12-04
 */

/*
 * Copyright (C) 2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _PART_BLK__DRIVER_H_
#define _PART_BLK__DRIVER_H_

#include <base/env.h>
#include <base/allocator_avl.h>
#include <base/signal.h>
#include <base/tslab.h>
#include <base/heap.h>
#include <util/list.h>
#include <block_session/connection.h>

namespace Block {
	class Block_dispatcher;
	class Driver;
};


class Block::Block_dispatcher
{
	public:

		virtual void dispatch(Packet_descriptor&, Packet_descriptor&) = 0;
};


bool operator== (const Block::Packet_descriptor& p1,
                 const Block::Packet_descriptor& p2)
{
	return p1.operation()    == p2.operation()    &&
	       p1.block_number() == p2.block_number() &&
	       p1.block_count()  == p2.block_count();
}


class Block::Driver
{
	public:

	class Request : public Genode::List<Request>::Element
	{
		private:

			Block_dispatcher &_dispatcher;
			Packet_descriptor _cli;
			Packet_descriptor _srv;

		public:

			Request(Block_dispatcher &d,
			        Packet_descriptor &cli,
			        Packet_descriptor &srv)
			: _dispatcher(d), _cli(cli), _srv(srv) {}

			bool handle(Packet_descriptor& reply)
			{
				bool ret =  reply == _srv;
				if (ret) _dispatcher.dispatch(_cli, reply);
				return ret;
			}
	};

	private:

		enum { BLK_SZ = Session::TX_QUEUE_SIZE*sizeof(Request) };

		Genode::Tslab<Request, BLK_SZ> _r_slab;
		Genode::List<Request>          _r_list;
		Genode::Allocator_avl          _block_alloc;
		Block::Connection              _session;
		Block::sector_t                _blk_cnt;
		Genode::size_t                 _blk_size;
		Genode::Signal_handler<Driver> _source_ack;
		Genode::Signal_handler<Driver> _source_submit;
		Block::Session::Operations     _ops;

		void _ready_to_submit();

		void _ack_avail()
		{
			/* check for acknowledgements */
			while (_session.tx()->ack_avail()) {
				Packet_descriptor p = _session.tx()->get_acked_packet();
				for (Request *r = _r_list.first(); r; r = r->next()) {
					if (r->handle(p)) {
						_r_list.remove(r);
						Genode::destroy(&_r_slab, r);
						break;
					}
				}
				_session.tx()->release_packet(p);
			}

			_ready_to_submit();
		}

	public:

		Driver(Genode::Entrypoint &ep, Genode::Heap &heap)
		: _r_slab(&heap),
		  _block_alloc(&heap),
		  _session(&_block_alloc, 4 * 1024 * 1024),
		  _source_ack(ep, *this, &Driver::_ack_avail),
		  _source_submit(ep, *this, &Driver::_ready_to_submit)
		{
			_session.info(&_blk_cnt, &_blk_size, &_ops);
		}

		Genode::size_t blk_size() { return _blk_size; }
		Genode::size_t blk_cnt()  { return _blk_cnt;  }
		Session::Operations ops() { return _ops; }
		Session_client& session() { return _session;  }

		void work_asynchronously()
		{
			_session.tx_channel()->sigh_ack_avail(_source_ack);
			_session.tx_channel()->sigh_ready_to_submit(_source_submit);
		}

		static Driver& driver();

		void io(bool write, sector_t nr, Genode::size_t cnt, void* addr,
		        Block_dispatcher &dispatcher, Packet_descriptor& cli)
		{
			if (!_session.tx()->ready_to_submit())
				throw Block::Session::Tx::Source::Packet_alloc_failed();

			Block::Packet_descriptor::Opcode op = write
			    ? Block::Packet_descriptor::WRITE
			    : Block::Packet_descriptor::READ;
			Genode::size_t size = _blk_size * cnt;
			Packet_descriptor p(_session.dma_alloc_packet(size),
			                    op,  nr, cnt);
			Request *r = new (&_r_slab) Request(dispatcher, cli, p);
			_r_list.insert(r);

			if (write)
				Genode::memcpy(_session.tx()->packet_content(p),
				               addr, size);

			_session.tx()->submit_packet(p);
		}
};

#endif /* _PART_BLK__DRIVER_H_ */
