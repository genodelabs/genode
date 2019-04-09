/*
 * \brief  Block-session driver for partition server
 * \author Stefan Kalkowski
 * \date   2013-12-04
 */

/*
 * Copyright (C) 2013-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _PART_BLOCK__DRIVER_H_
#define _PART_BLOCK__DRIVER_H_

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


struct Block::Block_dispatcher : Genode::Interface
{
	virtual void dispatch(Packet_descriptor&, Packet_descriptor&) = 0;
};


bool operator== (const Block::Packet_descriptor& p1,
                 const Block::Packet_descriptor& p2)
{
	return p1.tag().value == p2.tag().value;
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
			        Packet_descriptor const &cli,
			        Packet_descriptor const &srv)
			: _dispatcher(d), _cli(cli), _srv(srv) {}

			bool handle(Packet_descriptor& reply)
			{
				bool ret = (reply == _srv);
				if (ret) _dispatcher.dispatch(_cli, reply);
				return ret;
			}

			bool same_dispatcher(Block_dispatcher &same) {
				return &same == &_dispatcher; }
	};

	private:

		enum { BLK_SZ = Session::TX_QUEUE_SIZE*sizeof(Request) };

		Genode::Tslab<Request, BLK_SZ> _r_slab;
		Genode::List<Request>          _r_list { };
		Genode::Allocator_avl          _block_alloc;
		Block::Connection              _session;
		Block::Session::Info     const _info { _session.info() };
		Genode::Signal_handler<Driver> _source_ack;
		Genode::Signal_handler<Driver> _source_submit;
		unsigned long                  _tag_cnt { 0 };

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

		Block::Session::Tag _alloc_tag()
		{
			/*
			 * The wrapping of '_tag_cnt' is no problem because the number
			 * of consecutive outstanding requests is much lower than the
			 * value range of tags.
			 */
			_tag_cnt++;
			return Block::Session::Tag { _tag_cnt };
		}

	public:

		Driver(Genode::Env &env, Genode::Heap &heap)
		: _r_slab(&heap),
		  _block_alloc(&heap),
		  _session(env, &_block_alloc, 4 * 1024 * 1024),
		  _source_ack(env.ep(), *this, &Driver::_ack_avail),
		  _source_submit(env.ep(), *this, &Driver::_ready_to_submit)
		{ }

		Genode::size_t blk_size()  const { return _info.block_size; }
		Genode::size_t blk_cnt()   const { return _info.block_count; }
		bool           writeable() const { return _info.writeable; }

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
			Genode::size_t const size = _info.block_size * cnt;
			Packet_descriptor p(_session.alloc_packet(size),
			                    op,  nr, cnt, _alloc_tag());
			Request *r = new (&_r_slab) Request(dispatcher, cli, p);
			_r_list.insert(r);

			if (write)
				Genode::memcpy(_session.tx()->packet_content(p),
				               addr, size);

			_session.tx()->submit_packet(p);
		}

		void sync_all(Block_dispatcher &dispatcher, Packet_descriptor &cli)
		{
			if (!_session.tx()->ready_to_submit())
				throw Block::Session::Tx::Source::Packet_alloc_failed();

			Packet_descriptor const p =
				Block::Session::sync_all_packet_descriptor(_info, _alloc_tag());

			_r_list.insert(new (&_r_slab) Request(dispatcher, cli, p));

			_session.tx()->submit_packet(p);
		}

		void remove_dispatcher(Block_dispatcher &dispatcher)
		{
			for (Request *r = _r_list.first(); r;) {
				if (!r->same_dispatcher(dispatcher)) {
					r = r->next();
					continue;
				}

				Request *remove = r;
				r = r->next();

				_r_list.remove(remove);
				Genode::destroy(&_r_slab, remove);
			}
		}
};

#endif /* _PART_BLOCK__DRIVER_H_ */
