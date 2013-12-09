/*
 * \brief  Test for the block session server side
 * \author Stefan Kalkowski
 * \date   2010-07-06
 *
 * This test app provides the framebuffer it requests via framebuffer session
 * as a block device.
 */

/*
 * Copyright (C) 2010-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#include <base/printf.h>
#include <cap_session/connection.h>
#include <framebuffer_session/connection.h>
#include <block/component.h>
#include <block/driver.h>

class Driver : public Block::Driver
{
	private:

		enum { BLOCK_SIZE = 512 };

		Framebuffer::Connection      _fb;
		Framebuffer::Mode            _fb_mode;
		Genode::Dataspace_capability _fb_cap;
		Genode::Dataspace_client     _fb_dsc;
		Genode::addr_t               _fb_addr;
		Genode::size_t               _fb_size;

	public:

		Driver()
		: _fb_mode(_fb.mode()),
		  _fb_cap(_fb.dataspace()),
		  _fb_dsc(_fb_cap),
		  _fb_addr(Genode::env()->rm_session()->attach(_fb_cap)),
		  _fb_size(_fb_dsc.size()){}


		/*******************************
		 **  Block::Driver interface  **
		 *******************************/

		Genode::size_t  block_size()  { return BLOCK_SIZE; }
		Block::sector_t block_count() { return _fb_size / BLOCK_SIZE; }

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
			/* sanity check block number */
			if (block_number + block_count > _fb_size / BLOCK_SIZE) {
				PWRN("Out of range: requested %zd blocks from block %llu",
				     block_count, block_number);
				return;
			}

			Genode::size_t offset = block_number * BLOCK_SIZE;
			Genode::size_t size   = block_count  * BLOCK_SIZE;

			Genode::memcpy((void*)buffer, (void*)(_fb_addr + offset), size);
			session->ack_packet(packet);
		}

		void write(Block::sector_t           block_number,
		           Genode::size_t            block_count,
		           char const               *buffer,
		           Block::Packet_descriptor &packet)
		{
			/* sanity check block number */
			if (block_number + block_count > _fb_size / BLOCK_SIZE) {
				PWRN("Out of range: requested %zd blocks from block %llu",
				     block_count, block_number);
				return;
			}

			Genode::size_t offset = block_number * BLOCK_SIZE;
			Genode::size_t size   = block_count  * BLOCK_SIZE;

			Genode::memcpy((void*)(_fb_addr + offset), (void*)buffer, size);
			_fb.refresh(0, 0, _fb_mode.width(), _fb_mode.height());
			session->ack_packet(packet);
		}
};


struct Factory : Block::Driver_factory
{
		Block::Driver *create() {
		return new (Genode::env()->heap()) Driver(); }

	void destroy(Block::Driver *driver) {
		Genode::destroy(Genode::env()->heap(), driver); }
};


int main()
{
	using namespace Genode;

	enum { STACK_SIZE = 2048 * sizeof(Genode::addr_t) };
	static Cap_connection cap;
	static Rpc_entrypoint ep(&cap, STACK_SIZE, "fb_block_ep");

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
