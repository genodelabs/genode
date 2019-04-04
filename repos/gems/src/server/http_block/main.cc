/*
 * \brief  Block interface for HTTP block driver
 * \author Sebastian Sumpf  <sebastian.sumpf@genode-labs.com>
 * \author Stefan Kalkowski  <stefan.kalkowski@genode-labs.com>
 * \date   2010-08-24
 */

/*
 * Copyright (C) 2010-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <base/attached_rom_dataspace.h>
#include <base/log.h>
#include <block/component.h>
#include <libc/component.h>

/* local includes */
#include "http.h"

using namespace Genode;

class Driver : public Block::Driver
{
	private:

		size_t _block_size;
		Http   _http;

	public:

		Driver(Heap &heap, Ram_allocator &ram,
		       size_t block_size, ::String const &uri)
		: Block::Driver(ram),
		  _block_size(block_size), _http(heap, uri) {}


		/*******************************
		 **  Block::Driver interface  **
		 *******************************/

		Block::Session::Info info() const override
		{
			return { .block_size  = _block_size,
			         .block_count = _http.file_size() / _block_size,
			         .align_log2  = log2(_block_size),
			         .writeable   = false };
		}

		void read(Block::sector_t           block_nr,
		          Genode::size_t            block_count,
		          char                     *buffer,
		          Block::Packet_descriptor &packet)
		{
			_http.cmd_get(block_nr * _block_size, block_count * _block_size,
			              (addr_t)buffer);
			ack_packet(packet);
		}
	};


class Factory : public Block::Driver_factory
{
	private:

		Env                   &_env;
		Heap                  &_heap;
		Attached_rom_dataspace _config { _env, "config" };
		::String         const _uri;
		size_t           const _blk_sz;

	public:

		Factory(Env &env, Heap &heap)
		:
			_env(env), _heap(heap),
			_uri   (_config.xml().attribute_value("uri", ::String())),
			_blk_sz(_config.xml().attribute_value("block_size", 512U))
		{
			log("Using file=", _uri, " as device with block size ",
			    Hex(_blk_sz, Hex::OMIT_PREFIX), ".");
		}

		Block::Driver *create() {
			return new (&_heap) Driver(_heap, _env.ram(), _blk_sz, _uri); }

	void destroy(Block::Driver *driver) {
		Genode::destroy(&_heap, driver); }
};


struct Main
{
	Env        &env;
	Heap        heap { env.ram(), env.rm() };
	Factory     factory { env, heap };
	Block::Root root { env.ep(), heap, env.rm(), factory, true };

	Main(Env &env) : env(env) {
		env.parent().announce(env.ep().manage(root)); }
};


void Libc::Component::construct(Libc::Env &env) { static Main m(env); }
