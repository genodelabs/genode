/*
 * \brief  Cache a block device
 * \author Stefan Kalkowski
 * \date   2013-12-05
 */

/*
 * Copyright (C) 2013-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <base/component.h>

#include "lru.h"
#include "driver.h"

using Policy = Lru_policy;
static Driver<Policy> * driver = nullptr;


/**
 * Synchronize a chunk with the backend device
 */
template <typename POLICY>
void Driver<POLICY>::Policy::sync(const typename POLICY::Element *e, char *dst)
{
	Cache::offset_t off =
		static_cast<const Driver<POLICY>::Chunk_level_4*>(e)->base_offset();

	if (!driver) throw Write_failed(off);

	if (!driver->blk()->tx()->ready_to_submit())
		throw Write_failed(off);
	try {
		Block::Packet_descriptor
			p(driver->blk()->dma_alloc_packet(Driver::CACHE_BLK_SIZE),
		      Block::Packet_descriptor::WRITE, off / driver->blk_sz(),
		      Driver::CACHE_BLK_SIZE / driver->blk_sz());
		driver->blk()->tx()->submit_packet(p);
	} catch(Block::Session::Tx::Source::Packet_alloc_failed) {
		throw Write_failed(off);
	}
}


struct Main
{
	template <typename T>
	struct Factory : Block::Driver_factory
	{
		Genode::Env  &env;
		Genode::Heap &heap;

		Factory(Genode::Env &env, Genode::Heap &heap) : env(env), heap(heap) {}

		Block::Driver *create()
		{
			driver = new (&heap) ::Driver<T>(env, heap);
			return driver;
		}

		void destroy(Block::Driver *driver) {
			Genode::destroy(&heap, static_cast<::Driver<T>*>(driver)); }
	};

	void resource_handler() { }

	Genode::Env                 &env;
	Genode::Heap                 heap    { env.ram(), env.rm()     };
	Factory<Lru_policy>          factory { env, heap               };
	Block::Root                  root    { env.ep(), heap, env.rm(), factory, true };
	Genode::Signal_handler<Main> resource_dispatcher {
		env.ep(), *this, &Main::resource_handler };

	Main(Genode::Env &env) : env(env)
	{
		env.parent().announce(env.ep().manage(root));
		env.parent().resource_avail_sigh(resource_dispatcher);
	}
};


void Component::construct(Genode::Env &env)
{
	/* XXX execute constructors of global statics */
	env.exec_static_constructors();

	static Main server(env);
}
