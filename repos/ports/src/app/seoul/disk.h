/*
 * \brief  Block interface
 * \author Markus Partheymueller
 * \author Alexander Boettcher
 * \date   2012-09-15
 */

/*
 * Copyright (C) 2012 Intel Corporation
 * Copyright (C) 2013 Genode Labs GmbH
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

#ifndef _VANCOUVER_DISK_H_
#define _VANCOUVER_DISK_H_

/* Genode includes */
#include <base/allocator_avl.h>
#include <base/printf.h>
#include <base/thread.h>
#include <block_session/connection.h>
#include <util/string.h>
#include <base/synced_allocator.h>

/* local includes */
#include "synced_motherboard.h"

class Vancouver_disk;

class Vancouver_disk_signal : public Genode::Signal_dispatcher<Vancouver_disk>
{
	private:

		unsigned _disk_nr;

	public:

		Vancouver_disk_signal(Genode::Signal_receiver &sig_rec,
			                  Vancouver_disk &obj,
		                      void (Vancouver_disk::*member)(unsigned),
		                      unsigned disk_nr)
		: Genode::Signal_dispatcher<Vancouver_disk>(sig_rec, obj, member),
		  _disk_nr(disk_nr) {}

		unsigned disk_nr() { return _disk_nr; }
};


class Vancouver_disk : public Genode::Thread_deprecated<8192>, public StaticReceiver<Vancouver_disk>
{
	private:

		/* helper class to lookup a MessageDisk object */
		class Avl_entry : public Genode::Avl_node<Avl_entry>
		{

			private:

				Genode::addr_t _key;
				MessageDisk *  _msg;

			public:

				Avl_entry(void * key, MessageDisk * msg)
				 : _key(reinterpret_cast<Genode::addr_t>(key)), _msg(msg) { }

				bool higher(Avl_entry *e) { return e->_key > _key; }

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
		struct {
			Block::Connection          *blk_con;
			Block::Session::Operations  ops;
			Genode::size_t              blk_size;
			Block::sector_t             blk_cnt;
			Vancouver_disk_signal      *dispatcher;
		} _diskcon[MAX_DISKS];

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

		Genode::Avl_tree<Avl_entry> _lookup_msg;
		Genode::Lock           _lookup_msg_lock;

		/* entry function if signal must be dispatched */
		void _signal_dispatch_entry(unsigned);

	public:

		/**
		 * Constructor
		 */
		Vancouver_disk(Synced_motherboard &,
		               char         * backing_store_base,
		               Genode::size_t backing_store_size);

		~Vancouver_disk();

		void entry();

		bool receive(MessageDisk &msg);

		void register_host_operations(Motherboard &);
};

#endif /* _VANCOUVER_DISK_H_ */
