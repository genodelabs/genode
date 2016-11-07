/*
 * \brief  Test block session interface - server side
 * \author Stefan Kalkowski
 * \date   2013-12-09
 */

/*
 * Copyright (C) 2013-2016 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#include <base/attached_rom_dataspace.h>
#include <block/component.h>
#include <block/driver.h>
#include <os/ring_buffer.h>
#include <timer_session/connection.h>


class Driver : public Block::Driver
{
	private:

		enum { MAX_REQUESTS = 5 };

		typedef Genode::Ring_buffer<Block::Packet_descriptor, MAX_REQUESTS,
		                            Genode::Ring_buffer_unsynchronized> Req_buffer;

		Genode::size_t                    _number;
		Genode::size_t                    _size;
		Req_buffer                        _packets;
		Genode::Ram_dataspace_capability  _blk_ds;
		unsigned char                    *_blk_buf;

	public:

		Driver(Genode::Env &env, Genode::size_t number, Genode::size_t size)
		: _number(number), _size(size),
		  _blk_ds(env.ram().alloc(number*size)),
		  _blk_buf(env.rm().attach(_blk_ds)) {}

		void handler()
		{
			while (!_packets.empty()) {
				Block::Packet_descriptor p = _packets.get();
				ack_packet(p);
			}
		}


		/*******************************
		 **  Block::Driver interface  **
		 *******************************/

		void session_invalidated() {
			while (!_packets.empty()) _packets.get(); }

		Genode::size_t  block_size()  { return _size;   }
		Block::sector_t block_count() { return _number; }

		Block::Session::Operations ops()
		{
			Block::Session::Operations ops;
			ops.set_operation(Block::Packet_descriptor::READ);
			ops.set_operation(Block::Packet_descriptor::WRITE);
			return ops;
		}

		void read(Block::sector_t           block_number,
		          Genode::size_t            block_count,
		          char                     *buffer,
		          Block::Packet_descriptor &packet)
		{
			if (!_packets.avail_capacity())
				throw Block::Driver::Request_congestion();

			Genode::memcpy((void*)buffer, &_blk_buf[block_number*_size],
			               block_count * _size);
			_packets.add(packet);
		}

		void write(Block::sector_t           block_number,
		           Genode::size_t            block_count,
		           const char               *buffer,
		           Block::Packet_descriptor &packet)
		{
			if (!_packets.avail_capacity())
				throw Block::Driver::Request_congestion();
			Genode::memcpy(&_blk_buf[block_number*_size],
			               (void*)buffer, block_count * _size);
			_packets.add(packet);
		}
};


struct Main
{
	Genode::Env &env;
	Genode::Heap heap { env.ram(), env.rm() };

	struct Factory : Block::Driver_factory
	{
		::Driver *driver;

		Factory(Genode::Env &env, Genode::Heap &heap)
		{
			Genode::size_t blk_nr = 1024;
			Genode::size_t blk_sz = 512;

			try {
				Genode::Attached_rom_dataspace config { env, "config" };
				config.xml().attribute("sectors").value(&blk_nr);
				config.xml().attribute("block_size").value(&blk_sz);
			}
			catch (...) { }

			driver = new (&heap) Driver(env, blk_nr, blk_sz);
		}

		Block::Driver *create() { return driver; }

		void destroy(Block::Driver *driver) { }
	} factory { env, heap };

	Block::Root                    root { env.ep(), heap, factory };
	Timer::Connection              timer;
	Genode::Signal_handler<Driver> dispatcher { env.ep(), *factory.driver,
	                                            &Driver::handler };

	Main(Genode::Env &env) : env(env)
	{
		timer.sigh(dispatcher);
		timer.trigger_periodic(10000);
		env.parent().announce(env.ep().manage(root));
	}
};


Genode::size_t Component::stack_size() {
	return 2048*sizeof(Genode::addr_t); }


void Component::construct(Genode::Env &env) {
	static Main server(env); }
