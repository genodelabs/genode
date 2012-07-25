/*
 * \brief  Block-session component
 * \author Christian Helmuth
 * \date   2011-05-20
 */

/*
 * Copyright (C) 2011-2012 Genode Labs GmbH
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

	class Session_component : public Session_rpc_object
	{
		private:

			enum { RQ_STACK_SIZE = 8192 };
			class Rq_thread : public Thread<RQ_STACK_SIZE>
			{
				private:

					Tx::Sink *_sink;
					Driver   &_driver;
					addr_t    _rq_phys; /* physical addr. of rq_ds */

				public:

					Rq_thread(Tx::Sink *sink, Driver &driver, addr_t rq_phys)
					:
						Thread<RQ_STACK_SIZE>("rq"),
						_sink(sink), _driver(driver), _rq_phys(rq_phys)
					{ start(); }

					void entry()
					{
						/* handle requests */
						while (true) {

							/* blocking-get packet from client */
							Packet_descriptor packet = _sink->get_packet();
							if (!packet.valid()) {
								PWRN("received invalid packet");
								continue;
							}

							packet.succeeded(true);

							switch (packet.operation()) {

								case Block::Packet_descriptor::READ:

									try {
										if (_driver.dma_enabled())
											_driver.read_dma(packet.block_number(), packet.block_count(),
											                 _rq_phys + packet.offset());
										else
											_driver.read(packet.block_number(), packet.block_count(),
											             _sink->packet_content(packet));
									} catch (Driver::Io_error) {
										packet.succeeded(false);
									}
									break;

								case Block::Packet_descriptor::WRITE:
									try {
										if (_driver.dma_enabled())
											_driver.write_dma(packet.block_number(), packet.block_count(),
											                  _rq_phys + packet.offset());
										else
											_driver.write(packet.block_number(), packet.block_count(),
											              _sink->packet_content(packet));
									} catch (Driver::Io_error) {
										packet.succeeded(false);
									}
									break;

								default:

									PWRN("received invalid packet");
									packet.succeeded(false);
									continue;
							}

							/* acknowledge packet to the client */
							if (!_sink->ready_to_ack())
								PDBG("need to wait until ready-for-ack");

							_sink->acknowledge_packet(packet);
						}
					}
			};

			Driver_factory       &_driver_factory;
			Driver               &_driver;
			Dataspace_capability  _rq_ds;
			Rq_thread             _rq_thread;

		public:

			/**
			 * Constructor
			 */
			Session_component(Dataspace_capability  rq_ds,
			                  Driver_factory       &driver_factory,
			                  Rpc_entrypoint       &ep)
			:
				Session_rpc_object(rq_ds, ep),
				_driver_factory(driver_factory),
				_driver(*_driver_factory.create()),
				_rq_ds(rq_ds),
				_rq_thread(tx_sink(), _driver, Dataspace_client(rq_ds).phys_addr())
			{ }

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
	};

	/*
	 * Shortcut for single-client root component
	 */
	typedef Root_component<Session_component, Single_client> Root_component;

	/**
	 * Root component, handling new session requests
	 */
	class Root : public Root_component
	{
		private:

			Driver_factory &_driver_factory;
			Rpc_entrypoint &_ep;

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

				return new (md_alloc())
					Session_component(env()->ram_session()->alloc(tx_buf_size, false),
					                  _driver_factory, _ep);
			}

		public:

			Root(Rpc_entrypoint *session_ep, Allocator *md_alloc,
			     Driver_factory &driver_factory)
			:
				Root_component(session_ep, md_alloc),
				_driver_factory(driver_factory), _ep(*session_ep)
			{ }
	};
}

#endif /* _INCLUDE__BLOCK__COMPONENT_H_ */
