/*
 * \brief  Block interface for HTTP block driver
 * \author Sebastian Sumpf  <sebastian.sumpf@genode-labs.com>
 * \date   2010-08-24
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
#include <root/component.h>
#include <os/config.h>

/* local includes */
#include "http.h"

using namespace Genode;

namespace Block {

	class Http_interface
	{
		private:

			size_t _block_size;
			Http  *_http;

		public:

			Http_interface() : _block_size(512), _http(0)  {}

			static Http_interface* obj()
			{
				static Http_interface _obj;
				return &_obj;
			}

			void block_size(size_t block_size)
			{
				_block_size = block_size;
			}

			size_t block_size() { return _block_size; }

			void base_addr(addr_t base_addr)
			{
				_http->base_addr(base_addr);
			}

			void read(size_t block_nr, size_t block_count, off_t offset)
			{
				_http->cmd_get(block_nr * _block_size, block_count * _block_size, offset);
			}

			size_t block_count()
			{
				return _http->file_size() / _block_size;
			}

			void uri(char *uri, size_t length)
			{
				_http = new(env()->heap()) Http(uri, length);
			}

			Http * http_blk() { return _http; }
	};

	class Session_component : public Session_rpc_object
	{
		private:

			class Tx_thread : public Genode::Thread<8192>
			{
				private:

					Session_component *_session;

				public:

					Tx_thread(Session_component *session)
					: Genode::Thread<8192>("worker"), _session(session) { }

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
									Http_interface::obj()->read(packet.block_number(),
									                            packet.block_count(),
									                            packet.offset());
									packet.succeeded(true);
								}
								catch (Http::Socket_error) { PERR("socket error"); }
								catch (Http::Server_error) { PERR("server error"); }

								break;

							case Block::Packet_descriptor::WRITE:
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

		public:

			/**
			 * Constructor
			 *
			 * \param tx_ds  dataspace used for tx channel
			 */
			Session_component(Genode::Dataspace_capability tx_ds,
			                  Genode::Rpc_entrypoint &ep)
			: Session_rpc_object(tx_ds, ep), _tx_ds(tx_ds),
			  _startup_sema(0), _tx_thread(this)
			{
				/*
				 * Map packet stream
				 */
				addr_t base = env()->rm_session()->attach(tx_ds);

				Http_interface::obj()->base_addr(base);

				_tx_thread.start();
				_startup_sema.down();
			}

			void info(Genode::size_t *blk_count, Genode::size_t *blk_size,
			          Operations *ops)
			{
				*blk_count = Http_interface::obj()->block_count();
				*blk_size  = Http_interface::obj()->block_size();
				ops->set_operation(Packet_descriptor::READ);
			}

			void sync() {}

			/**
			 * Signal indicating that transmit thread is ready
			 */
			void tx_ready() { _startup_sema.up(); }
	};

	/*
	 * Allow one client only
	 */

	/*
	 * Shortcut for single-client root component
	 */
	typedef Genode::Root_component<Session_component> Root_component;

	/**
	 * Root component, handling new session requests
	 */
	class Root : public Root_component
	{
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

				return new (md_alloc())
				       Session_component(env()->ram_session()->alloc(tx_buf_size), *ep());
			}

		public:

			Root(Genode::Rpc_entrypoint *session_ep,
			     Genode::Allocator *md_alloc)
			: Root_component(session_ep, md_alloc) { }
	};


}


static void process_config()
{
	using namespace Genode;

	Xml_node config_node = config()->xml_node();

	bool uri = false;
	for (unsigned i = 0; i < config_node.num_sub_nodes(); ++i) {

		Xml_node file_node = config_node.sub_node(i);

		if (file_node.has_type("uri")) {
			Block::Http_interface::obj()->uri(file_node.content_addr(), file_node.content_size());
			uri = true;
		}

		if (file_node.has_type("block-size")) {
			size_t blk_size;
			file_node.value(&blk_size);
			Block::Http_interface::obj()->block_size(blk_size);
		}
	}

	if (!uri)
		throw Http::Uri_error();
}


int main()
{
	enum { STACK_SIZE = 4*1024 };
	static Cap_connection cap;
	static Rpc_entrypoint ep(&cap, STACK_SIZE, "http_block_ep");

	process_config();

	static Block::Root block_root(&ep, env()->heap());
	env()->parent()->announce(ep.manage(&block_root));
	sleep_forever();

	return 0;
}
