/*
 * \brief  Block interface for ATA driver
 * \author Stefan Kalkowski <stefan.kalkowski@genode-labs.com>
 * \author Sebastian Sumpf  <sebastian.sumpf@genode-labs.com>
 * \date   2010-07-16
 */

/*
 * Copyright (C) 2010-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <base/thread.h>
#include <base/sleep.h>
#include <base/semaphore.h>
#include <block_session/rpc_object.h>
#include <cap_session/connection.h>
#include <os/config.h>
#include <root/component.h>
#include <util/xml_node.h>

/* local includes */
#include "ata_device.h"
#include "mindrvr.h"

using namespace Genode;
using namespace Ata;

namespace Block {

	class Session_component : public Session_rpc_object
	{
		private:

			class Tx_thread : public Genode::Thread<8192>
			{
				private:

					Session_component *_session;

				public:

					Tx_thread(Session_component *session)
					: Thread("block_session_tx"), _session(session) { }

					void entry()
					{
						using namespace Genode;

						Session_component::Tx::Sink *tx_sink = _session->tx_sink();
						Block::Packet_descriptor packet;

						_session->tx_ready();

						/* handle requests */
						while (true) {

							/* blocking-get packet from client */
							packet = tx_sink->get_packet();
							if (!packet.valid()) {
								PWRN("received invalid packet");
								continue;
							}

							packet.succeeded(false);

							switch (packet.operation()) {

								case Block::Packet_descriptor::READ:

									try {
										_session->device()->read(packet.block_number(),
										                         packet.block_count(),
										                         packet.offset());
										packet.succeeded(true);
									}
									catch (Ata::Device::Io_error) {
										PWRN("Io error!");
									}
									break;

								case Block::Packet_descriptor::WRITE:
									try {
										_session->device()->write(packet.block_number(),
										                          packet.block_count(),
										                          packet.offset());
										packet.succeeded(true);
									}
									catch (Ata::Device::Io_error) {
										PWRN("Io error!");
									}
									break;

								default:
									PWRN("received invalid packet");
									continue;
							}

							/* acknowledge packet to the client */
							if (!tx_sink->ready_to_ack())
								PDBG("need to wait until ready-for-ack");
							tx_sink->acknowledge_packet(packet);
						}
					}
			};

		private:

			Genode::Dataspace_capability _tx_ds;         /* buffer for tx channel */
			Genode::Semaphore            _startup_sema;  /* thread startup sync */
			Tx_thread                    _tx_thread;
			Ata::Device                 *_device;

		public:

			/**
			 * Constructor
			 *
			 * \param tx_ds  dataspace used for tx channel
			 * \param ep     entry point used for packet stream
			 */
			Session_component(Genode::Dataspace_capability tx_ds,
			                  Genode::Rpc_entrypoint &ep,
			                  Ata::Device *device)
			: Session_rpc_object(tx_ds, ep), _tx_ds(tx_ds),
			  _startup_sema(0), _tx_thread(this), _device(device)
			{
				/*
				 * Determine I/O-buffer address. Map for LBA,
				 * use physical address for DMA
				 */
				addr_t base;
				if (!_device->dma_enabled()) {
					base = env()->rm_session()->attach(tx_ds);
				}
				else {
					Genode::Dataspace_client client(tx_ds);
					base = client.phys_addr();
				}

				_device->set_base_addr(base);

				_tx_thread.start();
				_startup_sema.down();
			}

			void info(Genode::size_t *blk_count, Genode::size_t *blk_size,
			          Operations *ops)
			{
				_device->read_capacity();
				*blk_count = _device->block_count();
				*blk_size  = _device->block_size();
				ops->set_operation(Packet_descriptor::READ);

				if (!(dynamic_cast<Atapi_device *>(_device)))
					ops->set_operation(Packet_descriptor::WRITE);
			}

			Ata::Device* device() { return _device; };

			/**
			 * Signal indicating that transmit thread is ready
			 */
			void tx_ready() { _startup_sema.up(); }
	};

	/*
	 * Shortcut for single-client root component
	 */
	typedef Genode::Root_component<Session_component,
	                               Genode::Single_client>
	        Root_component;

	/**
	 * Root component, handling new session requests
	 */
	class Root : public Root_component
	{
		private:

			Genode::Rpc_entrypoint &_ep;
			Ata::Device *_device;

		protected:

			/**
			 * Always returns the singleton block-session component
			 */
			Session_component *_create_session(const char *args)
			{
				using namespace Genode;

				Genode::size_t ram_quota =
					Arg_string::find_arg(args, "ram_quota"  ).ulong_value(0);
				Genode::size_t tx_buf_size =
					Arg_string::find_arg(args, "tx_buf_size").ulong_value(0);

				/* delete ram quota by the memory needed for the session */
				Genode::size_t session_size = max((Genode::size_t)4096,
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

				/* determine if we probe for ATA or ATAPI */
				bool probe_ata  = false;
				try {
					probe_ata = Genode::config()->xml_node().attribute("ata").has_value("yes"); }
				catch (...) {}

				int type = probe_ata ? REG_CONFIG_TYPE_ATA : REG_CONFIG_TYPE_ATAPI;

				/*
				 * Probe for ATA(PI) device, but only once
				 */
				if (!_device)
					_device = Ata::Device::probe_legacy(type);

				if (!_device) {
					PERR("No device present");
					throw Root::Unavailable();
				}

				if (Atapi_device *atapi_device = dynamic_cast<Atapi_device *>(_device))
					if (!atapi_device->test_unit_ready()) {
						PERR("No disc present");
						throw Root::Unavailable();
					}

				return new (md_alloc())
					Session_component(env()->ram_session()->alloc(tx_buf_size), _ep, _device);
			}

		public:

			Root(Genode::Rpc_entrypoint *session_ep,
			     Genode::Allocator *md_alloc)
			: Root_component(session_ep, md_alloc), _ep(*session_ep), _device(0) { }
	};
}


int main()
{
	enum { STACK_SIZE = 8192 };
	static Cap_connection cap;
	static Rpc_entrypoint ep(&cap, STACK_SIZE, "atapi_ep");

	static Block::Root block_root(&ep, env()->heap());
	env()->parent()->announce(ep.manage(&block_root));
	sleep_forever();
	return 0;
}
