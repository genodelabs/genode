/*
 * \brief  Block-session implementation for USB storage
 * \author Sebastian Sumpf <sebastian.sumpf@genode-labs.com>
 * \date   2012-05-23
 */

/*
 * Copyright (C) 2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _STORAGE__COMPONENT_H_
#define _STORAGE__COMPONENT_H_

#include <root/component.h>
#include <block_session/rpc_object.h>

#include "signal.h"

namespace Block {

	using namespace Genode;

	class Session_component;

	struct Device
	{
		/**
		 * Request block size for driver and medium
		 */
		virtual Genode::size_t block_size()  = 0;

		/**
		 * Request capacity of medium in blocks
		 */
		virtual Genode::size_t block_count() = 0;

		virtual void io(Session_component *session, Packet_descriptor &packet,
		                addr_t virt, addr_t phys) = 0;
	};


	template <typename T>
	class Signal_dispatcher : public  Driver_context,
                            public  Signal_context_capability
	{
		private:

			T &obj;
			void (T::*member) ();
			Signal_receiver *sig_rec;

		public:

			/**
			 * Constructor
			 *
			 * \param sig_rec     signal receiver to associate the signal
			 *                    handler with
			 * \param obj,member  object and member function to call when
			 *                    the signal occurs
			 */
			Signal_dispatcher(Signal_receiver *sig_rec,
			                  T &obj, void (T::*member)())
			:
				Signal_context_capability(sig_rec->manage(this)),
				obj(obj), member(member),
				sig_rec(sig_rec)
			{ }

			~Signal_dispatcher() { sig_rec->dissolve(this); }

			void handle() { (obj.*member)(); }
			char const *debug() { return "Block_context"; }
	};


	class Session_component : public Session_rpc_object
	{
		private:

			addr_t  _rq_phys ; /* physical addr. of rq_ds */
			Device *_device;   /* device this session is using */

			Signal_dispatcher<Session_component> _process_packet_dispatcher;

			void _process_packets()
			{
				while (tx_sink()->packet_avail())
				{
					Packet_descriptor packet = tx_sink()->get_packet();
					addr_t virt = (addr_t)tx_sink()->packet_content(packet);
					addr_t phys = _rq_phys + packet.offset();

					try {
						_device->io(this, packet, virt, phys);
					} catch (...) { PERR("Failed to queue packet"); }
				}
			}

		public:

			/**
			 * Constructor
			 */
			Session_component(Dataspace_capability  rq_ds,
			                  Rpc_entrypoint       &ep,
			                  Signal_receiver *sig_rec,
			                  Device *device)
			:
				Session_rpc_object(rq_ds, ep),
				_rq_phys(Dataspace_client(rq_ds).phys_addr()),
				_device(device),
				_process_packet_dispatcher(sig_rec, *this,
				                           &Session_component::_process_packets)
			{ 
				/*
				 * Register '_process_packets' dispatch function as signal
				 * handler for packet-avail and ready-to-ack signals.
				 */
				_tx.sigh_packet_avail(_process_packet_dispatcher);
				_tx.sigh_ready_to_ack(_process_packet_dispatcher);
			}

			void info(size_t *blk_count, size_t *blk_size,
			          Operations *ops)
			{
				*blk_count = _device->block_count();
				*blk_size  = _device->block_size();
				ops->set_operation(Packet_descriptor::READ);
				ops->set_operation(Packet_descriptor::WRITE);
			}

			void complete(Packet_descriptor &packet, bool success)
			{
				packet.succeeded(success);
				tx_sink()->acknowledge_packet(packet);
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

			Rpc_entrypoint  &_ep;
			Signal_receiver *_sig_rec;
			Device          *_device;

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
				 * Check if donated ram quota suffices for both communication
				 * buffers. Also check both sizes separately to handle a
				 * possible overflow of the sum of both sizes.
				 */
				if (tx_buf_size > ram_quota - session_size) {
					PERR("insufficient 'ram_quota', got %zd, need %zd",
					     ram_quota, tx_buf_size + session_size);
					throw Root::Quota_exceeded();
				}

				return new (md_alloc())
					Session_component(env()->ram_session()->alloc(tx_buf_size),
					                  _ep, _sig_rec, _device);
			}

		public:

			Root(Rpc_entrypoint  *session_ep, Allocator *md_alloc,
			     Signal_receiver *sig_rec, Device *device)
			:
				Root_component(session_ep, md_alloc),
				_ep(*session_ep), _sig_rec(sig_rec), _device(device)
			{ }
	};
}

#endif /* _STORAGE__COMPONENT_H_ */
