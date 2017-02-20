/*
 * \brief  Cache driver
 * \author Stefan Kalkowski
 * \date   2013-12-05
 */

/*
 * Copyright (C) 2013-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <base/log.h>
#include <block_session/connection.h>
#include <block/component.h>
#include <os/packet_allocator.h>
#include <os/server.h>

#include "chunk.h"

/**
 * Cache driver used by the generic block driver framework
 *
 * \param POLICY  the cache replacement policy (e.g. LRU)
 */
template <typename POLICY>
class Driver : public Block::Driver
{
	private:

		/**
		 * This class encapsulates requests to the backend device in progress,
		 * and the packets from the client side that triggered the request.
		 */
		struct Request : public Genode::List<Request>::Element
		{
			Block::Packet_descriptor srv;
			Block::Packet_descriptor cli;
			char * const             buffer;

			Request(Block::Packet_descriptor &s,
			        Block::Packet_descriptor &c,
			        char * const              b)
				: srv(s), cli(c), buffer(b) {}

			/*
			 * \return true when the given response packet matches
			 *         the request send to the backend device
			 */
			bool match(const Block::Packet_descriptor& reply) const
			{
				return reply.operation()    == srv.operation()  &&
				       reply.block_number() == srv.block_number() &&
				       reply.block_count()  == srv.block_count();
			}

			/*
			 * \param write  whether it's a write or read request
			 * \param nr     block number requested
			 * \param cnt    number of blocks requested
			 * \return true when the given parameters match
			 *         the request send to the backend device
			 */
			bool match(const bool write,
			           const Block::sector_t nr,
			           const Genode::size_t  cnt) const
			{
				Block::Packet_descriptor::Opcode op = write
					? Block::Packet_descriptor::WRITE
					: Block::Packet_descriptor::READ;
				return op == srv.operation()    &&
				       nr >= srv.block_number() &&
				       nr+cnt <= srv.block_number()+srv.block_count();
			}
		};


		/**
		 * Write failed exception at a specific device offset,
		 * can be triggered whenever the backend device is not ready
		 * to proceed
		 */
		struct Write_failed : Genode::Exception
		{
			Cache::offset_t off;

			Write_failed(Cache::offset_t o) : off(o) {}
		};


		/*
		 * The given policy class is extended by a synchronization routine,
		 * used by the cache chunk structure
		 */
		struct Policy : POLICY {
			static void sync(const typename POLICY::Element *e, char *src); };

	public:

		enum {
			SLAB_SZ = Block::Session::TX_QUEUE_SIZE*sizeof(Request),
			CACHE_BLK_SIZE = 4096
		};

		/**
		 * We use five levels of page-table like chunk structure,
		 * thereby we've a maximum device size of 256^4*4096 (LBA48)
		 */
		typedef Cache::Chunk<CACHE_BLK_SIZE, Policy>           Chunk_level_4;
		typedef Cache::Chunk_index<256, Chunk_level_4, Policy> Chunk_level_3;
		typedef Cache::Chunk_index<256, Chunk_level_3, Policy> Chunk_level_2;
		typedef Cache::Chunk_index<256, Chunk_level_2, Policy> Chunk_level_1;
		typedef Cache::Chunk_index<256, Chunk_level_1, Policy> Chunk_level_0;

	private:

		Genode::Env                      &_env;
		Genode::Tslab<Request, SLAB_SZ>   _r_slab;    /* slab for requests  */
		Genode::List<Request>             _r_list;    /* list of requests   */
		Genode::Packet_allocator          _alloc;     /* packet allocator   */
		Block::Connection                 _blk;       /* backend device     */
		Block::Session::Operations        _ops;       /* allowed operations */
		Genode::size_t                    _blk_sz;    /* block size         */
		Block::sector_t                   _blk_cnt;   /* block count        */
		Chunk_level_0                     _cache;     /* chunk hierarchy    */
		Genode::Signal_handler<Driver>    _source_ack;
		Genode::Signal_handler<Driver>    _source_submit;
		Genode::Signal_handler<Driver>    _yield;

		Driver(Driver const&);            /* singleton pattern */
		Driver& operator=(Driver const&); /* singleton pattern */

		/*
		 * Return modulus of cache's versus backend device's block size
		 */
		inline int _cache_blk_mod() { return CACHE_BLK_SIZE / _blk_sz; }

		/*
		 * Round off given block number to cache block size granularity
		 */
		inline Block::sector_t _cache_blk_round_off(Block::sector_t nr) {
			return nr - (nr % _cache_blk_mod()); }

		/*
		 * Round up given block number to cache block size granularity
		 */
		inline Block::sector_t _cache_blk_round_up(Block::sector_t nr) {
			return (nr % _cache_blk_mod())
			       ? nr + _cache_blk_mod() - (nr % _cache_blk_mod())
			       : nr; }

		/*
		 * Handle response to a single request
		 *
		 * \param srv  packet received from the backend device
		 * \param r    outstanding request
		 */
		inline void _handle_reply(Block::Packet_descriptor &srv, Request *r)
		{
			try {
			if (r->cli.operation() == Block::Packet_descriptor::READ)
				read(r->cli.block_number(), r->cli.block_count(),
				     r->buffer, r->cli);
			else
				write(r->cli.block_number(), r->cli.block_count(),
				      r->buffer, r->cli);
			} catch(Block::Driver::Request_congestion) {
				Genode::warning("cli (", r->cli.block_number(), " ",
				                         r->cli.block_count(), ") "
				                "srv (", r->srv.block_number(), " ",
				                         r->srv.block_count(), ")");
			}
		}

		/*
		 * Handle acknowledgements from the backend device
		 */
		void _ack_avail()
		{
			while (_blk.tx()->ack_avail()) {
				Block::Packet_descriptor p = _blk.tx()->get_acked_packet();

				/* when reading, write result into cache */
				if (p.operation() == Block::Packet_descriptor::READ)
					_cache.write(_blk.tx()->packet_content(p),
					             p.block_count() * _blk_sz,
					             p.block_number() * _blk_sz);

				/* loop through the list of requests, and ack all related */
				for (Request *r = _r_list.first(), *r_to_handle = r; r;
				     r_to_handle = r) {
					r = r->next();
					if (r_to_handle->match(p)) {
						_handle_reply(p, r_to_handle);
						_r_list.remove(r_to_handle);
						Genode::destroy(&_r_slab, r_to_handle);
					}
				}

				_blk.tx()->release_packet(p);
			}
		}

		/*
		 * Handle that the backend device is ready to receive again
		 */
		void _ready_to_submit() { }

		/*
		 * Setup a request to the backend device
		 *
		 * \param block_number block number offset
		 * \param block_count  number of blocks
		 * \param packet       original packet request received from the client
		 */
		void _request(Block::sector_t           block_number,
		              Genode::size_t            block_count,
		              char * const              buffer,
		              Block::Packet_descriptor &packet)
		{
			Block::Packet_descriptor p_to_dev;

			try {
				/* we've to look whether the request is already pending */
				for (Request *r = _r_list.first(); r; r = r->next()) {
					if (r->match(false, block_number, block_count)) {
						_r_list.insert(new (&_r_slab) Request(r->srv, packet,
						                                      buffer));
						return;
					}
				}

				/* it doesn't pay, we've to send a request to the device */
				if (!_blk.tx()->ready_to_submit()) {
					Genode::warning("not ready_to_submit");
					throw Request_congestion();
				}

				/* read ahead CACHE_BLK_SIZE */
				Block::sector_t nr = _cache_blk_round_off(block_number);
				Genode::size_t cnt = _cache_blk_round_up(block_count +
				                                         (block_number - nr));

				/* ensure all memory is available before sending the request */
				_cache.alloc(cnt * _blk_sz, nr * _blk_sz);

				/* construct and send the packet */
				p_to_dev =
					Block::Packet_descriptor(_blk.dma_alloc_packet(_blk_sz*cnt),
					                         Block::Packet_descriptor::READ,
					                         nr, cnt);
				_r_list.insert(new (&_r_slab) Request(p_to_dev, packet, buffer));
				_blk.tx()->submit_packet(p_to_dev);
			} catch(Block::Session::Tx::Source::Packet_alloc_failed) {
				throw Request_congestion();
			} catch(Genode::Allocator::Out_of_memory) {
				/* clean up */
				_blk.tx()->release_packet(p_to_dev);
				throw Request_congestion();
			}
		}

		/*
		 * Synchronize dirty chunks with backend device
		 */
		void _sync()
		{
			Cache::offset_t off = 0;
			Cache::size_t len   = _blk_sz * _blk_cnt;

			while (len > 0) {
				try {
					_cache.sync(len, off);
					len = 0;
				} catch(Write_failed &e) {
					/**
					 * Write to backend failed when backend device isn't ready
					 * to proceed, so handle signals, until it's ready again
					 */
					off = e.off;
					len = _blk_sz * _blk_cnt - off;
					_env.ep().wait_and_dispatch_one_signal();
				}
			}
		}

		/*
		 * Check for chunk availability
		 *
		 * \param nr   block number offset
		 * \param cnt  number of blocks
		 * \param p    client side packet, which triggered this operation
		 */
		bool _stat(Block::sector_t nr, Genode::size_t cnt,
		           char * const buffer, Block::Packet_descriptor &p)
		{
			Cache::offset_t off   = nr  * _blk_sz;
			Cache::size_t   size  = cnt * _blk_sz;
			Cache::offset_t end   = off + size;

			try {
				_cache.stat(size, off);
				return true;
			} catch(Cache::Chunk_base::Range_incomplete &e) {
				off  = Genode::max(off, e.off);
				size = Genode::min(end - off, e.size);
				_request(off / _blk_sz, size / _blk_sz, buffer, p);
			}
			return false;
		}

		/*
		 * Signal handler for yield requests of the parent
		 */
		void _parent_yield()
		{
			using namespace Genode;

			Parent::Resource_args const args = _env.parent().yield_request();
			size_t const requested_ram_quota =
				Arg_string::find_arg(args.string(), "ram_quota").ulong_value(0);

			/* flush the requested amount of RAM from cache */
			POLICY::flush(requested_ram_quota);
			_env.parent().yield_response();
		}

	public:

		/*
		 * Constructor
		 *
		 * \param ep  server entrypoint
		 */
		Driver(Genode::Env &env, Genode::Heap &heap)
		: Block::Driver(env.ram()),
		  _env(env),
		  _r_slab(&heap),
		  _alloc(&heap, CACHE_BLK_SIZE),
		  _blk(_env, &_alloc, Block::Session::TX_QUEUE_SIZE*CACHE_BLK_SIZE),
		  _blk_sz(0),
		  _blk_cnt(0),
		  _cache(heap, 0),
		  _source_ack(env.ep(), *this, &Driver::_ack_avail),
		  _source_submit(env.ep(), *this, &Driver::_ready_to_submit),
		  _yield(env.ep(), *this, &Driver::_parent_yield)
		{
			using namespace Genode;

			_blk.info(&_blk_cnt, &_blk_sz, &_ops);
			_blk.tx_channel()->sigh_ack_avail(_source_ack);
			_blk.tx_channel()->sigh_ready_to_submit(_source_submit);
			env.parent().yield_sigh(_yield);

			if (CACHE_BLK_SIZE % _blk_sz) {
				error("only devices that block size is divider of ",
				      Hex(CACHE_BLK_SIZE, Hex::OMIT_PREFIX) ," supported");
				throw Io_error();
			}

			/* truncate chunk structure to real size of the device */
			_cache.truncate(_blk_sz*_blk_cnt);
		}

		~Driver()
		{
			/* when session gets closed, synchronize and flush the cache */
			_sync();
			POLICY::flush();
		}

		Block::Session_client* blk()    { return &_blk;   }
		Genode::size_t         blk_sz() { return _blk_sz; }


		/****************************
		 ** Block-driver interface **
		 ****************************/

		Genode::size_t  block_size()     { return _blk_sz;  }
		Block::sector_t block_count()    { return _blk_cnt; }
		Block::Session::Operations ops() { return _ops;     }

		void read(Block::sector_t           block_number,
		          Genode::size_t            block_count,
		          char*                     buffer,
		          Block::Packet_descriptor &packet)
		{
			if (!_ops.supported(Block::Packet_descriptor::READ))
				throw Io_error();

			if (!_stat(block_number, block_count, buffer, packet))
				return;

			_cache.read(buffer, block_count*_blk_sz, block_number*_blk_sz);
			ack_packet(packet);
		}

		void write(Block::sector_t           block_number,
		           Genode::size_t            block_count,
		           const char *              buffer,
		           Block::Packet_descriptor &packet)
		{
			if (!_ops.supported(Block::Packet_descriptor::WRITE))
				throw Io_error();

			_cache.alloc(block_count * _blk_sz, block_number * _blk_sz);

			if ((block_number % _cache_blk_mod()) &&
			    !_stat(block_number, 1, const_cast<char* const>(buffer), packet))
				return;

			if (((block_number+block_count) % _cache_blk_mod())
				&& !_stat(block_number+block_count-1, 1,
				          const_cast<char* const>(buffer), packet))
				return;

			_cache.write(buffer, block_count * _blk_sz,
			             block_number * _blk_sz);
			ack_packet(packet);
		}

		void sync() { _sync(); }
};
