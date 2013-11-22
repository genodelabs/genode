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
#include <base/allocator_avl.h>
#include <cap_session/connection.h>
#include <root/component.h>
#include <util/arg_string.h>
#include <util/misc_math.h>
#include <block_session/rpc_object.h>
#include <framebuffer_session/connection.h>
#include <timer_session/connection.h>
#include <base/semaphore.h>

static Genode::size_t fb_size = 0;  /* framebuffer size */
static Genode::addr_t fb_addr = 0;  /* start address of framebuffer */

namespace Block {

	class Session_component : public Session_rpc_object
	{
		private:

			enum { BLOCK_SIZE = 512 };

			/**
			 * Thread handling the requests of an open block session.
			 */
			class Tx_thread : public Genode::Thread<8192>
			{
				private:

					Session_component *_session;

				public:

					/**
					 * Constructor
					 */
					Tx_thread(Session_component *session)
					: Thread("block_session_tx"), _session(session) { }

					/**
					 * Thread entry function.
					 */
					void entry()
					{
						using namespace Genode;

						Session_component::Tx::Sink *tx_sink = _session->tx_sink();
						Block::Packet_descriptor packet;

						/* signal to server activation: we're ready */
						_session->tx_ready();

						/* handle requests */
						while (true) {

							/* blocking-get packet from client */
							packet = tx_sink->get_packet();
							if (!packet.valid()) {
								PWRN("received invalid packet");
								continue;
							}

							/* sanity check block number */
							if ((packet.block_number() + packet.block_count()
								 > fb_size / BLOCK_SIZE)
								|| packet.block_number() < 0) {
								PWRN("requested %zd blocks from block %zd",
									 packet.block_count(), packet.block_number());
								PWRN("out of range!");
								continue;
							}

							Genode::size_t offset = packet.block_number() * BLOCK_SIZE;
							Genode::size_t size   = packet.block_count()  * BLOCK_SIZE;
							switch (packet.operation()) {
							case Block::Packet_descriptor::READ:
								{
									/* copy content to packet payload */
									memcpy(tx_sink->packet_content(packet),
										   (void*)(fb_addr + offset),
										   size);
									break;
								}
							case Block::Packet_descriptor::WRITE:
								{
									/* copy content from packet payload */
									memcpy((void*)(fb_addr + offset),
										   tx_sink->packet_content(packet),
										   size);
									break;
								}
							default:
								PWRN("Unsupported operation %d", packet.operation());
								continue;
							}

							/* mark success of operation */
							packet.succeeded(true);

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

		public:

			/**
			 * Constructor
			 *
			 * \param tx_buf_size  buffer size for tx channel
			 */
			Session_component(Genode::size_t tx_buf_size,
			                  Genode::Rpc_entrypoint &ep)
			: Session_rpc_object(Genode::env()->ram_session()->alloc(tx_buf_size), ep),
			  _startup_sema(0), _tx_thread(this)
			{
				_tx_thread.start();
				_startup_sema.down();
			}


			void info(Genode::size_t *blk_count, Genode::size_t *blk_size,
			          Operations *ops)
			{
				*blk_count = fb_size / BLOCK_SIZE;
				*blk_size  = BLOCK_SIZE;
				ops->set_operation(Packet_descriptor::READ);
				ops->set_operation(Packet_descriptor::WRITE);
			}

			void sync() {}

			/** Signal that transmit thread is ready */
			void tx_ready() { _startup_sema.up(); }
	};

	/*
	 * Block-session component is a singleton,
	 * only one client should use the driver directly.
	 */
	static Session_component *_session = 0;

	/**
	 * Shortcut for single-client root component
	 */
	typedef Genode::Root_component<Session_component, Genode::Single_client>
	Root_component;

	/**
	 * Root component, handling new session requests
	 */
	class Root : public Root_component
	{
		private:

			Genode::Rpc_entrypoint &_ep;

		protected:

			/**
			 * Always returns the singleton block-session component
			 */
			Session_component *_create_session(const char *args)
			{
				using namespace Genode;

				if (Block::_session) {
					PERR("Support for single session only! Aborting...");
					throw Root::Unavailable();
				}

				Genode::size_t ram_quota =
					Arg_string::find_arg(args, "ram_quota").ulong_value(0);
				Genode::size_t tx_buf_size =
					Arg_string::find_arg(args, "tx_buf_size").ulong_value(0);

				/* delete ram quota by the memory needed for the session */
				Genode::size_t session_size = max((Genode::size_t)4096,
				                                  sizeof(Session_component));
				if (ram_quota < session_size)
					throw Root::Quota_exceeded();

				/*
				 * Check if donated ram quota suffices for the
				 * communication buffer.
				 */
				if (tx_buf_size > ram_quota - session_size) {
					PERR("insufficient 'ram_quota', got %zd, need %zd",
					     ram_quota, tx_buf_size + session_size);
					throw Root::Quota_exceeded();
				}

				Block::_session = new (md_alloc()) Session_component(tx_buf_size, _ep);
				return Block::_session;
			}

		public:

			Root(Genode::Rpc_entrypoint *session_ep,
			     Genode::Allocator      *md_alloc)
			: Root_component(session_ep, md_alloc), _ep(*session_ep) { }
	};
}


int main()
{
	using namespace Genode;

	Framebuffer::Connection fb;
	Dataspace_capability    fb_cap = fb.dataspace();
	Dataspace_client        dsc(fb_cap);

	fb_addr = env()->rm_session()->attach(fb_cap);
	fb_size = dsc.size();

	enum { STACK_SIZE = 4096 };
	static Cap_connection cap;
	static Rpc_entrypoint ep(&cap, STACK_SIZE, "fb_block_ep");

	static Block::Root block_root(&ep, env()->heap());
	env()->parent()->announce(ep.manage(&block_root));

	Timer::Connection timer;
	while (true) {
		fb.refresh(0,0,1024,768);
		timer.msleep(50);
	}
	return 0;
}
