/*
 * \brief  Block-session component
 * \author Christian Helmuth
 * \author Stefan Kalkowski
 * \date   2011-05-20
 */

/*
 * Copyright (C) 2011-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__BLOCK__COMPONENT_H_
#define _INCLUDE__BLOCK__COMPONENT_H_

#include <root/component.h>
#include <block_session/rpc_object.h>
#include <block/driver.h>

namespace Block {

	using namespace Genode;

	class Session_component;
	class Root;
};


class Block::Session_component : public Block::Session_rpc_object
{
	private:

		Driver_factory                      &_driver_factory;
		Driver                              &_driver;
		Ram_dataspace_capability             _rq_ds;
		addr_t                               _rq_phys;
		Signal_dispatcher<Session_component> _sink_ack;
		Signal_dispatcher<Session_component> _sink_submit;

		void _ready_to_submit(unsigned)
		{
			/* handle requests */
			while (tx_sink()->packet_avail()) {

				/* blocking-get packet from client */
				Packet_descriptor packet = tx_sink()->get_packet();
				if (!packet.valid()) {
					PWRN("received invalid packet");
					continue;
				}

				packet.succeeded(true);

				try {
					switch (packet.operation()) {

					case Block::Packet_descriptor::READ:
						if (_driver.dma_enabled())
							_driver.read_dma(packet.block_number(),
							                 packet.block_count(),
							                 _rq_phys + packet.offset());
						else
							_driver.read(packet.block_number(),
							             packet.block_count(),
							             tx_sink()->packet_content(packet));
						break;

					case Block::Packet_descriptor::WRITE:
						if (_driver.dma_enabled())
							_driver.write_dma(packet.block_number(),
							                  packet.block_count(),
							                  _rq_phys + packet.offset());
						else
							_driver.write(packet.block_number(),
							              packet.block_count(),
							              tx_sink()->packet_content(packet));
						break;

					default:
						PWRN("received invalid packet");
						packet.succeeded(false);
						continue;
					}
				} catch (Driver::Io_error) {
					packet.succeeded(false);
				}

				/* acknowledge packet to the client */
				if (!tx_sink()->ready_to_ack()) {
					PWRN("need to wait until ready-for-ack");
					return;
				}

				tx_sink()->acknowledge_packet(packet);
			}
		}

		void _ack_avail(unsigned) { }

	public:

		/**
		 * Constructor
		 */
		Session_component(Ram_dataspace_capability  rq_ds,
		                  Driver                   &driver,
		                  Driver_factory           &driver_factory,
		                  Rpc_entrypoint           &ep,
		                  Signal_receiver          &receiver)
		:
			Session_rpc_object(rq_ds, ep),
			_driver_factory(driver_factory),
			_driver(driver),
			_rq_ds(rq_ds),
			_rq_phys(Dataspace_client(_rq_ds).phys_addr()),
			_sink_ack(receiver, *this, &Session_component::_ack_avail),
			_sink_submit(receiver, *this,
			             &Session_component::_ready_to_submit)
		{
			_tx.sigh_ready_to_ack(_sink_ack);
			_tx.sigh_packet_avail(_sink_submit);
		}

		/**
		 * Destructor
		 */
		~Session_component()
		{
			_driver_factory.destroy(&_driver);
		}

		void info(size_t *blk_count, size_t *blk_size,
		          Operations *ops)
		{
			*blk_count = _driver.block_count();
			*blk_size  = _driver.block_size();
			ops->set_operation(Packet_descriptor::READ);
			ops->set_operation(Packet_descriptor::WRITE);
		}

		void sync() { _driver.sync(); }
};


/**
 * Root component, handling new session requests
 */
class Block::Root :
	public Genode::Root_component<Block::Session_component, Single_client>
{
	private:

		Driver_factory  &_driver_factory;
		Rpc_entrypoint  &_ep;
		Signal_receiver &_receiver;

	protected:

		/**
		 * Always returns the singleton block-session component
		 */
		Session_component *_create_session(const char *args)
		{
			size_t ram_quota =
				Arg_string::find_arg(args, "ram_quota"  ).ulong_value(0);
			size_t tx_buf_size =
				Arg_string::find_arg(args, "tx_buf_size").ulong_value(0);

			/* delete ram quota by the memory needed for the session */
			size_t session_size = max((size_t)4096,
			                          sizeof(Session_component)
			                          + sizeof(Allocator_avl));
			if (ram_quota < session_size)
				throw Root::Quota_exceeded();

			/*
			 * Check if donated ram quota suffices for both
			 * communication buffers. Also check both sizes separately
			 * to handle a possible overflow of the sum of both sizes.
			 */
			if (tx_buf_size > ram_quota - session_size) {
				PERR("insufficient 'ram_quota', got %zd, need %zd",
				     ram_quota, tx_buf_size + session_size);
				throw Root::Quota_exceeded();
			}

			Driver * driver = _driver_factory.create();
			Ram_dataspace_capability ds_cap;
			ds_cap = driver->alloc_dma_buffer(tx_buf_size);
			return new (md_alloc())
				Session_component(ds_cap, *driver, _driver_factory, _ep,
				                  _receiver);
		}

	public:

		Root(Rpc_entrypoint *session_ep, Allocator *md_alloc,
		     Driver_factory &driver_factory, Signal_receiver &receiver)
		:
			Root_component(session_ep, md_alloc),
			_driver_factory(driver_factory), _ep(*session_ep),
			_receiver(receiver)
		{ }
};

#endif /* _INCLUDE__BLOCK__COMPONENT_H_ */
