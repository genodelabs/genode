/*
 * \brief  Test block session interface - server side
 * \author Stefan Kalkowski
 * \date   2013-12-09
 */

/*
 * Copyright (C) 2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#include <base/printf.h>
#include <cap_session/connection.h>
#include <timer_session/connection.h>
#include <block/component.h>
#include <block/driver.h>
#include <os/config.h>
#include <os/ring_buffer.h>


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

		Driver(Genode::size_t number, Genode::size_t size)
		: _number(number), _size(size),
		  _blk_ds(Genode::env()->ram_session()->alloc(number*size)),
		  _blk_buf(Genode::env()->rm_session()->attach(_blk_ds)) {}

		void handler(unsigned)
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
	Server::Entrypoint       &ep;

	struct Factory : Block::Driver_factory
	{
		::Driver *driver;

		Factory()
		{
			Genode::size_t blk_nr = 1024;
			Genode::size_t blk_sz = 512;

			try {
				Genode::config()->xml_node().attribute("sectors").value(&blk_nr);
				Genode::config()->xml_node().attribute("block_size").value(&blk_sz);
			}
			catch (...) { }

			driver = new (Genode::env()->heap()) Driver(blk_nr, blk_sz);
		}

		Block::Driver *create() { return driver; }

		void destroy(Block::Driver *driver) { }
	} factory;

	Block::Root                       root;
	Timer::Connection                 timer;
	Server::Signal_rpc_member<Driver> dispatcher = { ep, *factory.driver,
	                                                 &Driver::handler };

	Main(Server::Entrypoint &ep)
	: ep(ep), root(ep, Genode::env()->heap(), factory)
	{
		timer.sigh(dispatcher);
		timer.trigger_periodic(10000);
		Genode::env()->parent()->announce(ep.manage(root));
	}
};


/************
 ** Server **
 ************/

namespace Server {
	char const *name()             { return "blk_srv_ep";        }
	size_t stack_size()            { return 2*1024*sizeof(long); }
	void construct(Entrypoint &ep) { static Main server(ep);     }
}
