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

#include <signal/dispatch.h>

namespace Block {

	using namespace Genode;

	class Session_component;

	struct Device : ::Device
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


	class Session_component : public Packet_session_component<Session_rpc_object>
	{
		private:

			addr_t  _rq_phys ; /* physical addr. of rq_ds */
			Device *_device;   /* device this session is using */

		protected:

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
			Session_component(Dataspace_capability  tx_ds,
			                  Ram_dataspace_capability  rx_ds,
			                  Rpc_entrypoint       &ep,
			                  Signal_receiver *sig_rec,
			                  ::Device *device)
			:
				Packet_session_component(tx_ds, ep, sig_rec),
				_rq_phys(Dataspace_client(tx_ds).phys_addr()),
				_device(static_cast<Device *>(device))
			{
				env()->ram_session()->free(rx_ds);
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
	class Root : public Packet_root<Root_component, Session_component>
	{
		public:

			Root(Rpc_entrypoint *session_ep, Allocator *md_alloc,
			     Signal_receiver *sig_rec, Device *device)
			:
				Packet_root(session_ep, md_alloc, sig_rec, device) { }
	};

}
#endif /* _STORAGE__COMPONENT_H_ */
