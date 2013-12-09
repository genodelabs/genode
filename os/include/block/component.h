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
		bool                                 _req_queue_full;
		Packet_descriptor                    _p_to_handle;
		unsigned                             _p_in_fly;

		/**
		 * Acknowledge a packet already handled
		 */
		inline void _ack_packet(Packet_descriptor &packet)
		{
			if (!tx_sink()->ready_to_ack())
				PERR("Not ready to ack!");

			tx_sink()->acknowledge_packet(packet);
			_p_in_fly--;
		}

		/**
		 * Range check packet request
		 */
		inline bool _range_check(Packet_descriptor &p) {
			return p.block_number() + p.block_count() - 1
			       < _driver.block_count(); }

		/**
		 * Handle a single request
		 */
		void _handle_packet(Packet_descriptor packet)
		{
			_p_to_handle = packet;
			_p_to_handle.succeeded(false);

			/* ignore invalid packets */
			if (!packet.valid() || !_range_check(_p_to_handle)) {
				_ack_packet(_p_to_handle);
				return;
			}

			try {
				switch (_p_to_handle.operation()) {

				case Block::Packet_descriptor::READ:
					if (_driver.dma_enabled())
						_driver.read_dma(packet.block_number(),
						                 packet.block_count(),
						                 _rq_phys + packet.offset(),
						                 _p_to_handle);
					else
						_driver.read(packet.block_number(),
						             packet.block_count(),
						             tx_sink()->packet_content(packet),
						             _p_to_handle);
					break;

				case Block::Packet_descriptor::WRITE:
					if (_driver.dma_enabled())
						_driver.write_dma(packet.block_number(),
						                  packet.block_count(),
						                  _rq_phys + packet.offset(),
						                  _p_to_handle);
					else
						_driver.write(packet.block_number(),
						              packet.block_count(),
						              tx_sink()->packet_content(packet),
						              _p_to_handle);
					break;

				default:
					throw Driver::Io_error();
				}
			} catch (Driver::Request_congestion) {
				_req_queue_full = true;
			} catch (Driver::Io_error) {
				_ack_packet(_p_to_handle);
			}
		}

		/**
		 * Triggered when a packet was placed into the empty submit queue
		 */
		void _packet_avail(unsigned)
		{
			/*
			 * as long as more packets are available, and we're able to ack
			 * them, and the driver's request queue isn't full,
			 * direct the packet request to the driver backend
			 */
			for (; !_req_queue_full && tx_sink()->packet_avail() &&
			     !(_p_in_fly >= tx_sink()->ack_slots_free()); _p_in_fly++)
					_handle_packet(tx_sink()->get_packet());
		}

		/**
		 * Triggered when an ack got removed from the full ack queue
		 */
		void _ready_to_ack(unsigned) { _packet_avail(0); }

	public:

		/**
		 * Constructor
		 *
		 * \param rq_ds           shared dataspace for packet stream
		 * \param driver          block driver backend
		 * \param driver_factory  factory to create and destroy driver objects
		 * \param ep              entrypoint handling this session component
		 * \param receiver        signal receiver managing signals of the client
		 */
		Session_component(Ram_dataspace_capability  rq_ds,
		                  Driver                   &driver,
		                  Driver_factory           &driver_factory,
		                  Rpc_entrypoint           &ep,
		                  Signal_receiver          &receiver)
		: Session_rpc_object(rq_ds, ep),
		  _driver_factory(driver_factory),
		  _driver(driver),
		  _rq_ds(rq_ds),
		  _rq_phys(Dataspace_client(_rq_ds).phys_addr()),
		  _sink_ack(receiver, *this, &Session_component::_ready_to_ack),
		  _sink_submit(receiver, *this, &Session_component::_packet_avail),
		  _req_queue_full(false),
		  _p_in_fly(0)
		{
			_tx.sigh_ready_to_ack(_sink_ack);
			_tx.sigh_packet_avail(_sink_submit);

			driver.session = this;
		}

		/**
		 * Acknowledges a packet processed by the driver to the client
		 *
		 * \param packet   the packet to acknowledge
		 * \param success  indicated whether the processing was successful
		 *
		 * \throw Ack_congestion
		 */
		void ack_packet(Packet_descriptor &packet, bool success = true)
		{
			bool ack_queue_full = _p_in_fly >= tx_sink()->ack_slots_free();

			packet.succeeded(success);
			_ack_packet(packet);

			if (!_req_queue_full && !ack_queue_full)
				return;

			/*
			 * when the driver's request queue was full,
			 * handle last unprocessed packet taken out of submit queue
			 */
			if (_req_queue_full) {
				_req_queue_full = false;
				_handle_packet(_p_to_handle);
			}

			/* resume packet processing */
			_packet_avail(0);
		}

		/**
		 * Destructor
		 */
		~Session_component() {
			_driver_factory.destroy(&_driver); }


		/*******************************
		 **  Block session interface  **
		 *******************************/

		void info(sector_t *blk_count, size_t *blk_size,
		          Operations *ops)
		{
			*blk_count = _driver.block_count();
			*blk_size  = _driver.block_size();
			*ops       = _driver.ops();
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

		/**
		 * Constructor
		 *
		 * \param session_ep      entrypoint handling this root component
		 * \param md_alloc        allocator to allocate session components
		 * \param driver_factory  factory to create and destroy driver backend
		 * \param receiver        signal receiver managing signals of the client
		 */
		Root(Rpc_entrypoint *session_ep, Allocator *md_alloc,
		     Driver_factory &driver_factory, Signal_receiver &receiver)
		:
			Root_component(session_ep, md_alloc),
			_driver_factory(driver_factory), _ep(*session_ep),
			_receiver(receiver)
		{ }
};

#endif /* _INCLUDE__BLOCK__COMPONENT_H_ */
