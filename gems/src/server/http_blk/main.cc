/*
 * \brief  Block interface for HTTP block driver
 * \author Sebastian Sumpf  <sebastian.sumpf@genode-labs.com>
 * \author Stefan Kalkowski  <stefan.kalkowski@genode-labs.com>
 * \date   2010-08-24
 */

/*
 * Copyright (C) 2010-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <cap_session/connection.h>
#include <block/component.h>
#include <os/config.h>

/* local includes */
#include "http.h"

using namespace Genode;

class Driver : public Block::Driver
{
	private:

		size_t _block_size;
		Http   _http;

	public:

		Driver(size_t block_size, char *uri)
		: _block_size(block_size), _http(uri) {}


		/*******************************
		 **  Block::Driver interface  **
		 *******************************/

		Genode::size_t  block_size()  { return _block_size; }
		Block::sector_t block_count() { return _http.file_size() / _block_size; }

		Block::Session::Operations ops()
		{
			Block::Session::Operations o;
			o.set_operation(Block::Packet_descriptor::READ);
			return o;
		}

		void read(Block::sector_t           block_nr,
		          Genode::size_t            block_count,
		          char                     *buffer,
		          Block::Packet_descriptor &packet)
		{
			_http.cmd_get(block_nr * _block_size, block_count * _block_size,
			              (addr_t)buffer);
			session->complete_packet(packet);
		}
	};


class Factory : public Block::Driver_factory
{
	private:

		char   _uri[64];
		size_t _blk_sz;

	public:

		Factory() : _blk_sz(512)
		{
			try {
				config()->xml_node().attribute("uri").value(_uri, sizeof(_uri));
				config()->xml_node().attribute("block_size").value(&_blk_sz);
			}
			catch (...) { }

			PINF("Using file=%s as device with block size %zx.", _uri, _blk_sz);
		}

		Block::Driver *create() {
			return new (env()->heap()) Driver(_blk_sz, _uri); }

	void destroy(Block::Driver *driver) {
		Genode::destroy(env()->heap(), driver); }
};


int main()
{
	enum { STACK_SIZE = 4*1024 };
	static Cap_connection cap;
	static Rpc_entrypoint ep(&cap, STACK_SIZE, "http_block_ep");

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
