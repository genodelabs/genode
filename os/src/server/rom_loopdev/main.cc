/*
 * \brief  Provide a rom-file as block device (aka loop devices)
 * \author Stefan Kalkowski
 * \date   2010-07-07
 */

/*
 * Copyright (C) 2010-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#include <base/exception.h>
#include <base/printf.h>
#include <base/sleep.h>
#include <base/semaphore.h>
#include <base/allocator_avl.h>
#include <root/component.h>
#include <os/config.h>
#include <cap_session/connection.h>
#include <rom_session/connection.h>
#include <block_session/rpc_object.h>

using namespace Genode;

namespace Block {

	class Session_component : public Session_rpc_object
	{
		private:

			/**
			 * Thread handling the requests of an open block session.
			 */
			class Tx_thread : public Thread<8192>
			{
				private:

					Session_component *_session;  /* corresponding session object */
					addr_t             _dev_addr; /* rom-file address */
					size_t             _dev_size; /* rom-file size */
					size_t             _blk_size; /* block size */

				public:

					/**
					 * Constructor
					 */
					Tx_thread(Session_component *session,
					          addr_t             dev_addr,
					          size_t             dev_size,
					          size_t             blk_size)
					: Thread("block_session_tx"),
					  _session(session),
					  _dev_addr(dev_addr),
					  _dev_size(dev_size),
					  _blk_size(blk_size) { }

					/**
					 * Thread's entry function.
					 */
					void entry()
					{
						Session_component::Tx::Sink *tx_sink =
							_session->tx_sink();
						Block::Packet_descriptor packet;

						/* signal preparedness to server activation */
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
								 > _dev_size / _blk_size)
								|| packet.block_number() < 0) {
								PWRN("requested blocks %zd-%zd out of range!",
									 packet.block_number(),
									 packet.block_number() + packet.block_count());
								continue;
							}

							size_t offset = packet.block_number() * _blk_size;
							size_t size   = packet.block_count()  * _blk_size;
							switch (packet.operation()) {
							case Block::Packet_descriptor::READ:
								{
									/* copy file content to packet payload */
									memcpy(tx_sink->packet_content(packet),
										   (void*)(_dev_addr + offset),
										   size);
									packet.succeeded(true);
									break;
								}
							case Block::Packet_descriptor::WRITE:
								{
									PWRN("write attempt on read-only device");
									packet.succeeded(false);
									break;
								}
							default:
								PWRN("unsupported operation");
								packet.succeeded(false);
								continue;
							}

							/* acknowledge packet to the client */
							if (!tx_sink->ready_to_ack())
								PDBG("need to wait until ready-for-ack");
							tx_sink->acknowledge_packet(packet);
						}
					}

					friend class Session_component;
			};

		private:

			Dataspace_capability _tx_ds;         /* buffer for tx channel */
			Semaphore            _startup_sema;  /* thread startup sync */
			Tx_thread            _tx_thread;     /* thread handling block requests */
			size_t               _block_count;   /* count of blocks of this session */
			size_t               _block_size;    /* size of a block */

		public:

			/**
			 * Constructor
			 *
			 * \param tx_buf_size  buffer size for tx channel
			 * \param dev_addr     address of attached file
			 * \param dev_size     size of attached file
			 */
			Session_component(size_t tx_buf_size,
			                  addr_t dev_addr,
			                  size_t dev_size,
			                  size_t blk_size,
			                  Genode::Rpc_entrypoint &ep)
			: Session_rpc_object(env()->ram_session()->alloc(tx_buf_size), ep),
			  _startup_sema(0),
			  _tx_thread(this, dev_addr, dev_size, blk_size),
			  _block_count(dev_size / blk_size),
			  _block_size(blk_size)
			{
				_tx_thread.start();

				/* block until thread is ready to handle requests */
				_startup_sema.down();
			}

			/**
			 * Signal indicating that transmit thread is ready
			 */
			void tx_ready() { _startup_sema.up(); }


			/*****************************
			 ** Block session interface **
			 *****************************/

			void info(size_t *blk_count, size_t *blk_size, Operations *ops)
			{
				*blk_count = _block_count;
				*blk_size  = _block_size;
				ops->set_operation(Block::Packet_descriptor::READ);
			}
	};


	/**
	 * Root component, handling new session requests
	 */
	class Root : public Root_component<Session_component>
	{
		private:

			Dataspace_capability    _file_cap;  /* rom-file capability */
			addr_t                  _file_addr; /* start address of attached file */
			size_t                  _file_sz;   /* file size */
			size_t                  _blk_sz;    /* block size */
			Genode::Rpc_entrypoint &_channel_ep;

		protected:

			Session_component *_create_session(const char *args)
			{
				size_t ram_quota =
					Arg_string::find_arg(args, "ram_quota"  ).ulong_value(0);
				size_t tx_buf_size =
					Arg_string::find_arg(args, "tx_buf_size").ulong_value(0);

				/*
				 * Check if donated ram quota suffices for session data,
				 * and communication buffer.
				 */
				size_t session_size = sizeof(Session_component) + tx_buf_size;
				if (max((size_t)4096, session_size) > ram_quota) {
					PERR("insufficient 'ram_quota', got %zd, need %zd",
					     ram_quota, session_size);
					throw Root::Quota_exceeded();
				}
				return new (md_alloc())
					Session_component(tx_buf_size, _file_addr, _file_sz, _blk_sz, _channel_ep);
			}

		public:

			/**
			 * Constructor
			 *
			 * \param session_ep  session entrypoint
			 * \param md_alloc    meta-data allocator
			 * \param file_cap    capabilty of file to use as block-device
			 */
			Root(Rpc_entrypoint *session_ep, Allocator *md_alloc,
			     Dataspace_capability file_cap, size_t blk_size)
			: Root_component<Session_component>(session_ep, md_alloc),
			  _file_cap(file_cap),
			  _file_addr(env()->rm_session()->attach(file_cap)),
			  _file_sz(Dataspace_client(file_cap).size()),
			  _blk_sz(blk_size),
			  _channel_ep(*session_ep) { }

			~Root() { env()->rm_session()->detach((void*)_file_addr); }
	};
}


/**
 * Get name of the file we want to provide as block device,
 * and the block_size.
 */
static void process_config(char *file, size_t size, size_t *blk_size)
{
	try {
		config()->xml_node().attribute("file").value(file, size);
		config()->xml_node().attribute("block_size").value(blk_size);
	}
	catch (...) { }
}


int main()
{
	static char   file[64];
	static size_t block_size = 512;

	process_config(file, sizeof(file), &block_size);

	PINF("Using file=%s as device with block size %zx.", file, block_size);

	try {
		enum { STACK_SIZE = 8192 };
		static Cap_connection cap;
		static Rpc_entrypoint ep(&cap, STACK_SIZE, "rom_loop_ep");
		static Rom_connection rom(file);
		static Block::Root blk_root(&ep, env()->heap(), rom.dataspace(), block_size);

		env()->parent()->announce(ep.manage(&blk_root));

		sleep_forever();
	} catch (Rom_connection::Rom_connection_failed) {
		PERR("Cannot open file %s.", file);
	}
	return 0;
}
