/*
 * \brief  Provide a rom-file as block device (aka loop devices)
 * \author Stefan Kalkowski
 * \date   2010-07-07
 */

/*
 * Copyright (C) 2010-2016 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#include <base/attached_rom_dataspace.h>
#include <base/component.h>
#include <base/log.h>
#include <block/component.h>
#include <rom_session/connection.h>

using namespace Genode;

class Rom_blk : public Block::Driver
{
	private:

		Env                 &_env;
		Rom_connection       _rom;
		size_t               _blk_sz;
		Dataspace_capability _file_cap  = _rom.dataspace();
		addr_t               _file_addr = _env.rm().attach(_file_cap);
		size_t               _file_sz   = Dataspace_client(_file_cap).size();
		size_t               _blk_cnt   = _file_sz / _blk_sz;

	public:

		using String = Genode::String<64UL>;

		Rom_blk(Env &env, String &name, size_t blk_sz)
		: _env(env), _rom(env, name.string()), _blk_sz(blk_sz) {}


		/****************************
		 ** Block-driver interface **
		 ****************************/

		size_t          block_size()  { return _blk_sz;  }
		Block::sector_t block_count() { return _blk_cnt; }

		Block::Session::Operations ops()
		{
			Block::Session::Operations o;
			o.set_operation(Block::Packet_descriptor::READ);
			return o;
		}

		void read(Block::sector_t           block_number,
		          size_t                    block_count,
		          char*                     buffer,
		          Block::Packet_descriptor &packet)
		{
			/* sanity check block number */
			if ((block_number + block_count > _file_sz / _blk_sz)
				|| block_number < 0) {
				warning("requested blocks ", block_number, "-",
				        block_number + block_count, " out of range!");
				return;
			}

			size_t offset = (size_t) block_number * _blk_sz;
			size_t size   = block_count  * _blk_sz;

			/* copy file content to packet payload */
			memcpy((void*)buffer, (void*)(_file_addr + offset), size);

			ack_packet(packet);
		}
};


struct Main
{
	Env &env;
	Heap heap { env.ram(), env.rm() };

	struct Factory : Block::Driver_factory
	{
		Env  &env;
		Heap &heap;

		Factory(Env &env, Heap &heap)
		: env(env), heap(heap) {}

		Block::Driver *create()
		{
			Rom_blk::String file;
			size_t blk_sz = 512;

			try {
				Attached_rom_dataspace config(env, "config");
				config.xml().attribute("file").value(&file);
				config.xml().attribute("block_size").value(&blk_sz);
			}
			catch (...) { }

			log("Using file=", file, " as device with block size ", blk_sz, ".");

			try {
				return new (&heap) Rom_blk(env, file, blk_sz);
			} catch(Rom_connection::Rom_connection_failed) {
				error("cannot open file ", file);
			}
			throw Root::Unavailable();
		}

		void destroy(Block::Driver *driver) {
			Genode::destroy(&heap, driver); }
	} factory { env, heap };

	Block::Root root { env.ep(), heap, factory };

	Main(Env &env) : env(env) {
		env.parent().announce(env.ep().manage(root)); }
};


Genode::size_t Component::stack_size()      { return 2*1024*sizeof(long); }
void Component::construct(Genode::Env &env) { static Main server(env);    }
