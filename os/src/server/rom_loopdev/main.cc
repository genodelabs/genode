/*
 * \brief  Provide a rom-file as block device (aka loop devices)
 * \author Stefan Kalkowski
 * \date   2010-07-07
 */

/*
 * Copyright (C) 2010-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#include <base/exception.h>
#include <base/printf.h>
#include <os/config.h>
#include <cap_session/connection.h>
#include <rom_session/connection.h>
#include <block/component.h>
#include <block/driver.h>

using namespace Genode;

class Rom_blk : public Block::Driver
{
	private:

		Rom_connection       _rom;
		Dataspace_capability _file_cap;  /* rom-file capability */
		addr_t               _file_addr; /* start address of attached file */
		size_t               _file_sz;   /* file size */
		size_t               _blk_sz;    /* block size */
		size_t               _blk_cnt;   /* block count */

	public:

		Rom_blk(const char *name, size_t blk_sz)
		: _rom(name),
		  _file_cap(_rom.dataspace()),
		  _file_addr(env()->rm_session()->attach(_file_cap)),
		  _file_sz(Dataspace_client(_file_cap).size()),
		  _blk_sz(blk_sz),
		  _blk_cnt(_file_sz/_blk_sz) { }


		/****************************
		 ** Block-driver interface **
		 ****************************/

		Genode::size_t block_size()  { return _blk_sz;  }
		Genode::size_t block_count() { return _blk_cnt; }

		Block::Session::Operations ops()
		{
			Block::Session::Operations o;
			o.set_operation(Block::Packet_descriptor::READ);
			return o;
		}

		void read(Genode::size_t  block_number,
		          Genode::size_t  block_count,
		          char           *out_buffer)
		{
			/* sanity check block number */
			if ((block_number + block_count > _file_sz / _blk_sz)
				|| block_number < 0) {
				PWRN("requested blocks %zd-%zd out of range!",
					 block_number, block_number + block_count);
				return;
			}

			size_t offset = block_number * _blk_sz;
			size_t size   = block_count  * _blk_sz;

			/* copy file content to packet payload */
			memcpy(out_buffer, (void*)(_file_addr + offset), size);
		}

		void write(Genode::size_t  block_number,
		           Genode::size_t  block_count,
		           char const     *buffer)
		{
			PWRN("write attempt on read-only device");
		}

		/*
		 * This driver does not support DMA operation, currently.
		 */

		void read_dma(Genode::size_t, Genode::size_t, Genode::addr_t) {
			throw Io_error(); }

		void write_dma(Genode::size_t, Genode::size_t, Genode::addr_t) {
			throw Io_error(); }

		bool dma_enabled() { return false; }

		Genode::Ram_dataspace_capability alloc_dma_buffer(Genode::size_t size) {
			return Genode::env()->ram_session()->alloc(size, false); }

		void sync() {}
};


struct Factory : Block::Driver_factory
{
	Block::Driver *create()
	{
		char   file[64];
		size_t blk_sz = 512;

		try {
			config()->xml_node().attribute("file").value(file, sizeof(file));
			config()->xml_node().attribute("block_size").value(&blk_sz);
		}
		catch (...) { }

		PINF("Using file=%s as device with block size %zx.", file, blk_sz);

		try {
			Rom_blk *driver = new (Genode::env()->heap()) Rom_blk(file, blk_sz);
			return driver;
		} catch(Rom_connection::Rom_connection_failed) {
			PERR("Cannot open file %s.", file);
		}
		throw Root::Unavailable();
	}

	void destroy(Block::Driver *driver) {
		Genode::destroy(env()->heap(), driver); }
};


int main()
{
	enum { STACK_SIZE = 8192 };
	static Cap_connection cap;
	static Rpc_entrypoint ep(&cap, STACK_SIZE, "rom_blk_ep");

	static Signal_receiver receiver;
	static Factory driver_factory;
	static Block::Root block_root(&ep, env()->heap(), driver_factory, receiver);

	env()->parent()->announce(ep.manage(&block_root));

	while (true) {
		Signal s = receiver.wait_for_signal();
		static_cast<Signal_dispatcher_base *>(s.context())->dispatch(s.num());
	}

	return 0;
}
