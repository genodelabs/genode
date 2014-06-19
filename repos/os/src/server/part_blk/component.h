/*
 * \brief  Block-session component for partition server
 * \author Stefan Kalkowski
 * \date   2013-12-04
 */

/*
 * Copyright (C) 2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _PART_BLK__COMPONENT_H_
#define _PART_BLK__COMPONENT_H_

#include <base/exception.h>
#include <root/component.h>
#include <block_session/rpc_object.h>

#include "partition_table.h"

namespace Block {

	using namespace Genode;

	class Session_component;
	class Root;
};


class Block::Session_component : public Block::Session_rpc_object,
                                 public List<Block::Session_component>::Element,
                                 public Block_dispatcher
{
	private:

		Ram_dataspace_capability             _rq_ds;
		addr_t                               _rq_phys;
		Partition                           *_partition;
		Signal_dispatcher<Session_component> _sink_ack;
		Signal_dispatcher<Session_component> _sink_submit;
		bool                                 _req_queue_full;
		bool                                 _ack_queue_full;
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
			return p.block_number() + p.block_count() <= _partition->sectors; }

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

			bool write   = _p_to_handle.operation() == Packet_descriptor::WRITE;
			sector_t off = _p_to_handle.block_number() + _partition->lba;
			size_t cnt   = _p_to_handle.block_count();
			void* addr   = tx_sink()->packet_content(_p_to_handle);
			try {
				Driver::driver().io(write, off, cnt, addr, *this, _p_to_handle);
			} catch (Block::Session::Tx::Source::Packet_alloc_failed) {
				_req_queue_full = true;
				Session_component::wait_queue().insert(this);
			}
		}

		/**
		 * Triggered when a packet was placed into the empty submit queue
		 */
		void _packet_avail(unsigned)
		{
			_ack_queue_full = _p_in_fly >= tx_sink()->ack_slots_free();

			/*
			 * as long as more packets are available, and we're able to ack
			 * them, and the driver's request queue isn't full,
			 * direct the packet request to the driver backend
			 */
			for (; !_req_queue_full && tx_sink()->packet_avail() &&
					 !_ack_queue_full; _p_in_fly++,
					 _ack_queue_full = _p_in_fly >= tx_sink()->ack_slots_free())
					_handle_packet(tx_sink()->get_packet());
		}

		/**
		 * Triggered when an ack got removed from the full ack queue
		 */
		void _ready_to_ack(unsigned) { _packet_avail(0); }

	public:

		/**
		 * Constructor
		 */
		Session_component(Ram_dataspace_capability  rq_ds,
		                  Partition                *partition,
		                  Rpc_entrypoint           &ep,
		                  Signal_receiver          &receiver)
		: Session_rpc_object(rq_ds, ep),
		  _rq_ds(rq_ds),
		  _rq_phys(Dataspace_client(_rq_ds).phys_addr()),
		  _partition(partition),
		  _sink_ack(receiver, *this, &Session_component::_ready_to_ack),
		  _sink_submit(receiver, *this, &Session_component::_packet_avail),
		  _req_queue_full(false),
		  _ack_queue_full(false),
		  _p_in_fly(0)
		{
			_tx.sigh_ready_to_ack(_sink_ack);
			_tx.sigh_packet_avail(_sink_submit);
		}

		Partition *partition() { return _partition; }

		void dispatch(Packet_descriptor &request, Packet_descriptor &reply)
		{
			if (request.operation() == Block::Packet_descriptor::READ) {
				void *src =
					Driver::driver().session().tx()->packet_content(reply);
				Genode::size_t sz =
					request.block_count() * Driver::driver().blk_size();
				Genode::memcpy(tx_sink()->packet_content(request), src, sz);
			}
			request.succeeded(true);
			_ack_packet(request);

			if (_ack_queue_full)
				_packet_avail(0);
		}

		static List<Session_component>& wait_queue()
		{
			static List<Session_component> l;
			return l;
		}

		static void wake_up()
		{
			for (Session_component *c = wait_queue().first(); c; c = c->next())
			{
				wait_queue().remove(c);
				c->_req_queue_full = false;
				c->_handle_packet(c->_p_to_handle);
				c->_packet_avail(0);
			}
		}

		/*******************************
		 **  Block session interface  **
		 *******************************/

		void info(sector_t *blk_count, size_t *blk_size,
		          Operations *ops)
		{
			*blk_count = _partition->sectors;
			*blk_size  = Driver::driver().blk_size();
			ops->set_operation(Packet_descriptor::READ);
			ops->set_operation(Packet_descriptor::WRITE);
		}

		void sync() { Driver::driver().session().sync(); }
};


/**
 * Root component, handling new session requests
 */
class Block::Root :
	public Genode::Root_component<Block::Session_component>
{
	private:

		Rpc_entrypoint  &_ep;
		Signal_receiver &_receiver;

		long _partition_num(const char *session_label)
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

			/* Search for configured partition number and the corresponding partition */
			char label_buf[64];
			Genode::Arg_string::find_arg(args,
			                             "label").string(label_buf,
			                                             sizeof(label_buf),
			                                             "<unlabeled>");
			long num = _partition_num(label_buf);
			if (num < 0) {
				PERR("No confguration found for client: %s", label_buf);
				throw Root::Invalid_args();
			}

			if (!Partition_table::table().partition(num)) {
				PERR("Partition %ld unavailable", num);
				throw Root::Unavailable();
			}

			Ram_dataspace_capability ds_cap;
			ds_cap = Genode::env()->ram_session()->alloc(tx_buf_size);
			return new (md_alloc())
				Session_component(ds_cap,
				                  Partition_table::table().partition(num),
				                  _ep, _receiver);
		}

	public:

		Root(Rpc_entrypoint *session_ep, Allocator *md_alloc,
		     Signal_receiver &receiver)
		:
			Root_component(session_ep, md_alloc),
			_ep(*session_ep),
			_receiver(receiver)
		{ }
};

#endif /* _PART_BLK__COMPONENT_H_ */
