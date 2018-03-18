/*
 * \brief  Block interface
 * \author Markus Partheymueller
 * \author Alexander Boettcher
 * \date   2012-09-15
 */

/*
 * Copyright (C) 2012 Intel Corporation
 * Copyright (C) 2013-2017 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 *
 * The code is partially based on the Vancouver VMM, which is distributed
 * under the terms of the GNU General Public License version 2.
 *
 * Modifications by Intel Corporation are contributed under the terms and
 * conditions of the GNU General Public License version 2.
 */

#ifndef _DISK_H_
#define _DISK_H_

/* Genode includes */
#include <base/allocator_avl.h>
#include <block_session/connection.h>
#include <util/string.h>
#include <base/synced_allocator.h>

/* local includes */
#include "synced_motherboard.h"

/* Seoul includes */
#include <host/dma.h>

namespace Seoul {
	class Disk;
	class Disk_signal;
}

class Seoul::Disk_signal
{
	private:

		Disk     &_obj;
		unsigned  _id;

		void _signal();

	public:

		Genode::Signal_handler<Disk_signal> const sigh;

		Disk_signal(Genode::Entrypoint &ep, Disk &obj,
		            Block::Connection &block, unsigned disk_nr)
		:
		  _obj(obj), _id(disk_nr),
		  sigh(ep, *this, &Disk_signal::_signal)
		{
			block.tx_channel()->sigh_ack_avail(sigh);
			block.tx_channel()->sigh_ready_to_submit(sigh);
		}
};


class Seoul::Disk : public StaticReceiver<Seoul::Disk>
{
	private:

		Genode::Env &_env;

		/* helper class to lookup a MessageDisk object */
		class Avl_entry : public Genode::Avl_node<Avl_entry>
		{

			private:

				Genode::addr_t  const _key;
				MessageDisk   * const _msg;

				/*
				 * Noncopyable
				 */
				Avl_entry(Avl_entry const &);
				Avl_entry &operator = (Avl_entry const &);

			public:

				Avl_entry(void * key, MessageDisk * const msg)
				 : _key(reinterpret_cast<Genode::addr_t>(key)), _msg(msg) { }

				bool higher(Avl_entry *e) const { return e->_key > _key; }

				Avl_entry *find(Genode::addr_t ptr)
				{
					if (ptr == _key) return this;
					Avl_entry *obj = this->child(ptr > _key);
					return obj ? obj->find(ptr) : 0;
				}
		
				MessageDisk * msg() { return _msg; }
		};

		/* block session used by disk models of VMM */
		enum { MAX_DISKS = 4 };
		struct disk_session {
			Block::Connection          *blk_con;
			Block::Session::Operations  ops;
			Genode::size_t              blk_size;
			Block::sector_t             blk_cnt;
			Disk_signal                *signal;
		} _diskcon[MAX_DISKS] { };

		Synced_motherboard &_motherboard;
		char        * const _backing_store_base;
		size_t        const _backing_store_size;

		/* slabs for temporary holding MessageDisk objects */
		typedef Genode::Tslab<MessageDisk, 128> MessageDisk_Slab;
		typedef Genode::Synced_allocator<MessageDisk_Slab> MessageDisk_Slab_Sync;

		MessageDisk_Slab_Sync _tslab_msg;

		/* Structure to find back the MessageDisk object out of a Block Ack */
		typedef Genode::Tslab<Avl_entry, 128> Avl_entry_slab;
		typedef Genode::Synced_allocator<Avl_entry_slab> Avl_entry_slab_sync;

		Avl_entry_slab_sync _tslab_avl;

		Genode::Avl_tree<Avl_entry> _lookup_msg  { };
		Genode::Avl_tree<Avl_entry> _restart_msg { };
		/* _alloc_lock protects both lists + alloc_packet/release_packet !!! */
		Genode::Lock                _alloc_lock  { };

		/*
		 * Noncopyable
		 */
		Disk(Disk const &);
		Disk &operator = (Disk const &);

		void check_restart();
		bool restart(struct disk_session const &, MessageDisk * const);
		bool execute(bool const write, struct disk_session const &,
		             MessageDisk const &);

		template <typename FN>
		bool check_dma_descriptors(MessageDisk const * const msg,
		                           FN const &fn)
		{
			/* check bounds for read and write operations */
			for (unsigned i = 0; i < msg->dmacount; i++) {
				char * const dma_addr = _backing_store_base +
				                        msg->dma[i].byteoffset +
				                        msg->physoffset;

				/* check for bounds */
				if (dma_addr >= _backing_store_base + _backing_store_size ||
				    dma_addr < _backing_store_base)
					return false;

				if (!fn(dma_addr, i))
					return false;
			}
			return true;
		}

		/* find the corresponding MessageDisk object */
		Avl_entry * lookup_and_remove(Genode::Avl_tree<Avl_entry> &tree,
		                              void * specific_obj = nullptr)
		{
			Genode::Lock::Guard lock_guard(_alloc_lock);

			Avl_entry * obj = tree.first();
			if (obj && specific_obj)
				obj = obj->find(reinterpret_cast<Genode::addr_t>(specific_obj));

			if (obj)
				tree.remove(obj);

			return obj;
		}

	public:

		/**
		 * Constructor
		 */
		Disk(Genode::Env &, Synced_motherboard &, char * backing_store_base,
		     Genode::size_t backing_store_size);

		void handle_disk(unsigned);

		bool receive(MessageDisk &msg);

		void register_host_operations(Motherboard &);
};

#endif /* _DISK_H_ */
