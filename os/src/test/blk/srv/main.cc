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

		typedef Ring_buffer<Block::Packet_descriptor, MAX_REQUESTS,
		                    Ring_buffer_unsynchronized> Req_buffer;

		Genode::size_t                    _number;
		Genode::size_t                    _size;
		Req_buffer                        _packets;
		Genode::Signal_dispatcher<Driver> _ack;
		Genode::Ram_dataspace_capability  _blk_ds;
		unsigned char                    *_blk_buf;

		void _handle_ack(unsigned)
		{
			while (!_packets.empty()) {
				Block::Packet_descriptor p = _packets.get();
				session->ack_packet(p);
			}
		}

	public:

		Driver(Genode::size_t number, Genode::size_t size,
		       Genode::Signal_receiver &receiver)
		: _number(number), _size(size),
		  _ack(receiver, *this, &Driver::_handle_ack),
		  _blk_ds(Genode::env()->ram_session()->alloc(number*size)),
		  _blk_buf(Genode::env()->rm_session()->attach(_blk_ds)) {}

		Genode::Signal_context_capability handler() { return _ack; }


		/*******************************
		 **  Block::Driver interface  **
		 *******************************/

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


struct Factory : Block::Driver_factory
{
	Genode::Signal_receiver &receiver;
	::Driver                *driver;

	Factory(Genode::Signal_receiver &r) : receiver(r)
	{
		Genode::size_t blk_nr = 1024;
		Genode::size_t blk_sz = 512;

		try {
			Genode::config()->xml_node().attribute("sectors").value(&blk_nr);
			Genode::config()->xml_node().attribute("block_size").value(&blk_sz);
		}
		catch (...) { }

		driver =  new (Genode::env()->heap()) Driver(blk_nr, blk_sz, receiver);
	}

		Block::Driver *create() { return driver; }

	void destroy(Block::Driver *driver) { }
};


int main()
{
	using namespace Genode;

	enum { STACK_SIZE = 2048 * sizeof(Genode::addr_t) };
	static Cap_connection cap;
	static Rpc_entrypoint ep(&cap, STACK_SIZE, "test_blk_ep");

	static Signal_receiver receiver;
	static Factory driver_factory(receiver);
	static Block::Root block_root(&ep, env()->heap(), driver_factory, receiver);

	env()->parent()->announce(ep.manage(&block_root));

	static Timer::Connection timer;
	timer.sigh(driver_factory.driver->handler());
	timer.trigger_periodic(10000);
	while (true) {
		Signal s = receiver.wait_for_signal();
		static_cast<Signal_dispatcher_base *>(s.context())->dispatch(s.num());
	}

	return 0;
}
