/*
 * \brief  Provide a RAM dataspace as writable block device
 * \author Stefan Kalkowski
 * \author Sebastian Sumpf
 * \author Josef Soentgen
 * \date   2010-07-07
 */

/*
 * Copyright (C) 2010-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <base/attached_ram_dataspace.h>
#include <base/attached_rom_dataspace.h>
#include <base/component.h>
#include <base/exception.h>
#include <base/heap.h>
#include <base/log.h>
#include <block/component.h>
#include <block/driver.h>


using namespace Genode;


class Ram_blk : public Block::Driver
{
	private:

		Env       &_env;
		Allocator *_alloc { nullptr };

		Attached_rom_dataspace *_rom_ds { nullptr };
		size_t                  _size;
		size_t                  _block_size;
		size_t                  _block_count;
		Attached_ram_dataspace  _ram_ds;
		addr_t                  _ram_addr;

		void _io(Block::sector_t           block_number,
		         size_t                    block_count,
		         char*                     buffer,
		         Block::Packet_descriptor &packet,
		         bool                      read)
		{
			/* sanity check block number */
			if (block_number + block_count > _block_count) {
				Genode::warning("requested blocks ", block_number, "-",
				                block_number + block_count," out of range!");
				return;
			}

			size_t offset = (size_t) block_number * _block_size;
			size_t size   = block_count  * _block_size;

			void *src = read ? (void *)(_ram_addr + offset) : (void *)buffer;
			void *dst = read ? (void *)buffer : (void *)(_ram_addr + offset);
			/* copy file content to packet payload */
			memcpy(dst, src, size);

			ack_packet(packet);
		}

	public:

		/**
		 * Construct populated RAM dataspace
		 */
		Ram_blk(Env &env, Allocator &alloc,
		        const char *name, size_t block_size)
		:	Block::Driver(env.ram()),
			_env(env), _alloc(&alloc),
			_rom_ds(new (_alloc) Attached_rom_dataspace(_env, name)),
			_size(_rom_ds->size()),
			_block_size(block_size),
			_block_count(_size/_block_size),
			_ram_ds(_env.ram(), _env.rm(), _size),
			_ram_addr((addr_t)_ram_ds.local_addr<addr_t>())
		{
			/* populate backing store from file */
			memcpy(_ram_ds.local_addr<void>(), _rom_ds->local_addr<void>(), _size);
		}

		/**
		 * Construct empty RAM dataspace
		 */
		Ram_blk(Env &env, size_t size, size_t block_size)
		:	Block::Driver(env.ram()),
			_env(env),
			_size(size),
			_block_size(block_size),
			_block_count(_size/_block_size),
			_ram_ds(_env.ram(), _env.rm(), _size),
			_ram_addr((addr_t)_ram_ds.local_addr<addr_t>())
		{ }

		~Ram_blk() { destroy(_alloc, _rom_ds); }


		/****************************
		 ** Block-driver interface **
		 ****************************/

		size_t  block_size()  { return _block_size;  }
		Block::sector_t block_count() { return _block_count; }

		Block::Session::Operations ops()
		{
			Block::Session::Operations o;
			o.set_operation(Block::Packet_descriptor::READ);
			o.set_operation(Block::Packet_descriptor::WRITE);
			return o;
		}

		void read(Block::sector_t    block_number,
		          size_t             block_count,
		          char*              buffer,
		          Block::Packet_descriptor &packet)
		{
			_io(block_number, block_count, buffer, packet, true);
		}

		void write(Block::sector_t  block_number,
		           size_t   block_count,
		           const char *     buffer,
		           Block::Packet_descriptor &packet)
		{
			_io(block_number, block_count, const_cast<char *>(buffer), packet, false);
		}
};


struct Main
{
	Env  &env;
	Heap  heap { env.ram(), env.rm() };

	Attached_rom_dataspace config_rom { env, "config" };

	struct Factory : Block::Driver_factory
	{
		Env       &env;
		Allocator &alloc;

		bool   use_file { false };
		char   file[64];

		size_t       size { 0 };
		size_t block_size { 512 };

		Factory(Env &env, Allocator &alloc,
		        Xml_node config)
		: env(env), alloc(alloc)
		{
			use_file = config.has_attribute("file");
			if (use_file) {
				config.attribute("file").value(file, sizeof(file));
			} else {
				try {
					Genode::Number_of_bytes bytes;
					config.attribute("size").value(&bytes);
					size = bytes;
				}
				catch (...) {
					error("neither file nor size attribute specified");
					throw Exception();
				}
			}

			block_size = config.attribute_value("block_size", block_size);
		}

		Block::Driver *create()
		{
			try {
				if (use_file) {
					Genode::log("Creating RAM-basd block device populated by file='",
					            Genode::Cstring(file), "' with block size ", block_size);
					return new (&alloc) Ram_blk(env, alloc, file, block_size);
				} else {
					Genode::log("Creating RAM-based block device with size ",
					            size, " and block size ", block_size);
					return new (&alloc) Ram_blk(env, size, block_size);
				}
			}
			catch (...) { throw Service_denied(); }
		}

		void destroy(Block::Driver *driver) {
			Genode::destroy(&alloc, driver); }
	} factory { env, heap, config_rom.xml() };

	Block::Root root { env.ep(), heap, env.rm(), factory };

	Main(Env &env) : env(env)
	{
		env.parent().announce(env.ep().manage(root));
	}
};


void Component::construct(Genode::Env &env) { static Main server(env); }
