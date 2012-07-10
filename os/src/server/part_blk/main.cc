/*
 * \brief  Front end of the partition server
 * \author Sebastian Sumpf
 * \date   2011-05-30
 */

/*
 * Copyright (C) 2011-2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#include <base/sleep.h>
#include <block_session/rpc_object.h>
#include <cap_session/connection.h>
#include <os/config.h>
#include <root/component.h>
#include "part_blk.h"

namespace Block {

	long partition_num(const char *session_label)
	{
		long num = -1;

		try {
			using namespace Genode;

			Xml_node policy = Genode::config()->xml_node().sub_node("policy");

			for (;; policy = policy.next("policy")) {
				char label_buf[64];
				policy.attribute("label").value(label_buf, sizeof(label_buf));

				if (Genode::strcmp(session_label, label_buf))
					continue;

				/* read partition attribute */
				policy.attribute("partition").value(&num);
				break;
			}
		} catch (...) {}

		return num;
	}


	class Session_component : public Session_rpc_object
	{
		private:

			class Tx_thread : public Genode::Thread<8192>
			{
				private:

					Session_component *_session;

				public:

					Tx_thread(Session_component *session)
					: _session(session) { }

					void entry()
					{
						using namespace Genode;

						Session_component::Tx::Sink *tx_sink = _session->tx_sink();
						Block::Packet_descriptor packet;

						/* handle requests */
						while (true) {

							/* blocking get packet from client */
							packet = tx_sink->get_packet();
							if (!packet.valid()) {
								PWRN("received invalid packet");
								continue;
							}

							packet.succeeded(false);
							bool write = false;

							switch (packet.operation()) {

							case Block::Packet_descriptor::WRITE:
								write  = true;

							case Block::Packet_descriptor::READ:

								try {
									_session->partition()->io(packet.block_number(),
									                          packet.block_count(),
									                          tx_sink->packet_content(packet),
									                          write);
									packet.succeeded(true);
								}
								catch (Partition::Io_error) {
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

			struct Partition::Partition  *_partition; /* partition belonging to this session */
			Genode::Dataspace_capability  _tx_ds;     /* buffer for tx channel */
			Tx_thread                     _tx_thread;

		public:

			Session_component(Genode::Dataspace_capability tx_ds,
			                  Partition::Partition        *partition,
			                  Genode::Rpc_entrypoint      &ep)
			:
				Session_rpc_object(tx_ds, ep),
				_partition(partition), _tx_ds(tx_ds), _tx_thread(this)
			{
				_tx_thread.start();
			}

			void info(Genode::size_t *blk_count, Genode::size_t *blk_size, Operations *ops)
			{
				*blk_count = _partition->_sectors;
				*blk_size  = Partition::blk_size();
				ops->set_operation(Packet_descriptor::READ);
				ops->set_operation(Packet_descriptor::WRITE);
			}

			Partition::Partition *partition() { return _partition; }
	};


	typedef Genode::Root_component<Session_component> Root_component;

	/**
	 * Root component, handling new session requests
	 */
	class Root : public Root_component
	{
		private:

			Genode::Rpc_entrypoint &_ep;

		protected:

			/**
			 * Always returns
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

				/* Search for configured partition number and the corresponding partition */
				char label_buf[64];

				Genode::Arg_string::find_arg(args, "label").string(label_buf, sizeof(label_buf), "<unlabeled>");

				long num = partition_num(label_buf);
				if (num < 0) {
					PERR("No confguration found for client: %s", label_buf);
					throw Root::Invalid_args();
				}

				if (!Partition::partition(num)) {
					PERR("Partition %ld unavailable", num);
					throw Root::Unavailable();
				}

				return new (md_alloc())
				       Session_component(env()->ram_session()->alloc(tx_buf_size),
				                         Partition::partition(num), _ep);
			}

		public:

			Root(Genode::Rpc_entrypoint *session_ep, Genode::Allocator *md_alloc)
			: Root_component(session_ep, md_alloc), _ep(*session_ep) { }
	};
}


int main()
{
	using namespace Genode;

	try {
		Partition::init();
	} catch (Partition::Io_error) {
		return -1;
	}

	enum { STACK_SIZE = 16384 };
	static Cap_connection cap;
	static Rpc_entrypoint ep(&cap, STACK_SIZE, "part_ep");
	static Block::Root block_root(&ep, env()->heap());

	env()->parent()->announce(ep.manage(&block_root));
	sleep_forever();
	return 0;
}
