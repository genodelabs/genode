/*
 * \brief  Genode C API config functions needed by OKLinux
 * \author Stefan Kalkowski
 * \date   2009-05-19
 *
 * OKLinux includes several stub drivers, used as frontends to the Genode
 * framework, e.g. framebuffer-driver or rom-file block-driver.
 * Due to the strong bond of these drivers with the Genode framework,
 * they should be configured using Genode's configuration format and
 * not the kernel commandline.
 * A valid configuration example can be found in 'config/init_config'
 * within this repository.
 */

/*
 * Copyright (C) 2009-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#include <base/env.h>
#include <base/printf.h>
#include <base/allocator_avl.h>
#include <block_session/connection.h>


extern "C" {
#include <genode/block.h>
#include <genode/config.h>
}

class Req_cache
{
	private:

		class Req_entry
		{
			public:

				void     *pkt;
				void     *req;

				Req_entry() : pkt(0), req(0) {}
				Req_entry(void *packet, void *request)
				: pkt(packet), req(request) {}
		};


		enum { MAX = 128 };

		Req_entry _cache[MAX];

		int _find(void *packet)
		{
			for (int i=0; i < MAX; i++)
				if (_cache[i].pkt == packet)
					return i;
			return -1;
		}

	public:

		void insert(void *packet, void *request)
		{
			int idx = _find(0);
			if (idx < 0)
				PERR("Req cache is full!");
			else
				_cache[idx] = Req_entry(packet, request);
		}

		void remove(void *packet, void **request)
		{
			int idx = _find(packet);

			if (idx < 0) {
				*request = 0;
				PERR("Req cache entry not found!");
			}
			*request = _cache[idx].req;
			_cache[idx].pkt = 0;
			_cache[idx].req = 0;
		}
};


enum Dimensions {
	TX_BUF_SIZE = 1024 * 1024
};


static void (*end_request)(void*, short, void*, unsigned long) = 0;
static Genode::size_t             blk_size = 0;
static Genode::size_t             blk_cnt = 0;
static Block::Session::Operations blk_ops;


static Req_cache *cache()
{
	static Req_cache _cache;
	return &_cache;
}


static Block::Connection *session()
{
	static Genode::Allocator_avl _alloc(Genode::env()->heap());
	static Block::Connection     _session(&_alloc, TX_BUF_SIZE);
	return &_session;
}



extern "C" {

	void genode_block_register_callback(void (*func)(void*, short,
	                                                 void*, unsigned long))
	{
		end_request = func;
	}


	void
	genode_block_geometry(unsigned long *cnt, unsigned long *sz,
	                      int *write, unsigned long *queue_sz)
	{
		if (!blk_size && !blk_cnt)
			session()->info(&blk_cnt, &blk_size, &blk_ops);
		*cnt      = blk_cnt;
		*sz       = blk_size;
		*queue_sz = session()->tx()->bulk_buffer_size();
		*write    = blk_ops.supported(Block::Packet_descriptor::WRITE) ? 1 : 0;
	}


	void* genode_block_request(unsigned long sz, void *req, unsigned long *offset)
	{
		try {
			Block::Packet_descriptor p = session()->tx()->alloc_packet(sz);
			void *addr = session()->tx()->packet_content(p);
			cache()->insert(addr, req);
			*offset = p.offset();
			return addr;
		} catch (Block::Session::Tx::Source::Packet_alloc_failed) { }
		return 0;
	}


	void genode_block_submit(unsigned long queue_offset, unsigned long size,
	                         unsigned long disc_offset, int write)
	{
		Genode::size_t sector     = disc_offset / blk_size;
		Genode::size_t sector_cnt = size / blk_size;
		Block::Packet_descriptor p(Block::Packet_descriptor(queue_offset, size),
		                           write ? Block::Packet_descriptor::WRITE
		                           : Block::Packet_descriptor::READ,
		                           sector, sector_cnt);
		session()->tx()->submit_packet(p);
	}


	void genode_block_collect_responses()
	{
		static bool avail = genode_config_block();
		if (avail) {
			void *req;
			while (session()->tx()->ack_avail()) {
				Block::Packet_descriptor packet = session()->tx()->get_acked_packet();
				void *addr = session()->tx()->packet_content(packet);
				bool write = packet.operation() == Block::Packet_descriptor::WRITE;
				cache()->remove(session()->tx()->packet_content(packet), &req);
				if (req && end_request)
					end_request(req, write, addr, packet.size());
				session()->tx()->release_packet(packet);
			}
		}
	}
} // extern "C"
