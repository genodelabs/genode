/*
 * \brief  Cache a block device
 * \author Stefan Kalkowski
 * \date   2013-12-05
 */

/*
 * Copyright (C) 2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#include <os/server.h>

#include "lru.h"
#include "driver.h"


template <typename POLICY> Driver<POLICY>* Driver<POLICY>::_instance = 0;


/**
 * Synchronize a chunk with the backend device
 */
template <typename POLICY>
void Driver<POLICY>::Policy::sync(const typename POLICY::Element *e, char *dst)
{
	Cache::offset_t off =
		static_cast<const Driver<POLICY>::Chunk_level_4*>(e)->base_offset();

	if (!Driver::instance()->blk()->tx()->ready_to_submit())
		throw Write_failed(off);
	try {
		Block::Packet_descriptor
			p(Driver::instance()->blk()->dma_alloc_packet(Driver::CACHE_BLK_SIZE),
		      Block::Packet_descriptor::WRITE,
		      off / Driver::instance()->blk_sz(),
		      Driver::CACHE_BLK_SIZE / Driver::instance()->blk_sz());
		Driver::instance()->blk()->tx()->submit_packet(p);
	} catch(Block::Session::Tx::Source::Packet_alloc_failed) {
		throw Write_failed(off);
	}
}


struct Main
{
	Server::Entrypoint &ep;

	struct Factory : Block::Driver_factory
	{
		Server::Entrypoint &ep;

		Factory(Server::Entrypoint &ep) : ep(ep) {}

		Block::Driver *create() { return Driver<Lru_policy>::instance(ep); }
		void destroy(Block::Driver *driver) { Driver<Lru_policy>::destroy(); }
	} factory;

	void resource_handler(unsigned) { }

	Block::Root                     root;
	Server::Signal_rpc_member<Main> resource_dispatcher;

	Main(Server::Entrypoint &ep)
	: ep(ep), factory(ep), root(ep, Genode::env()->heap(), factory),
	  resource_dispatcher(ep, *this, &Main::resource_handler)
	{
		Genode::env()->parent()->announce(ep.manage(root));
		Genode::env()->parent()->resource_avail_sigh(resource_dispatcher);
	}
};


/************
 ** Server **
 ************/

namespace Server {
	char const *name()             { return "blk_cache_ep";      }
	size_t stack_size()            { return 2*1024*sizeof(long); }
	void construct(Entrypoint &ep) { static Main server(ep);     }
}
