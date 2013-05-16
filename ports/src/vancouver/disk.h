/*
 * \brief  Block interface
 * \author Markus Partheymueller
 * \date   2012-09-15
 */

/*
 * Copyright (C) 2012 Intel Corporation
 * Copyright (C) 2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
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
#include <base/printf.h>
#include <base/sleep.h>
#include <base/thread.h>
#include <block_session/connection.h>
#include <util/string.h>

/* local includes */
#include <synced_motherboard.h>

/* NOVA userland includes */
#include <host/dma.h>

static const bool read_only = false;


class Vancouver_disk : public Genode::Thread<8192>, public StaticReceiver<Vancouver_disk>
{
	private:

		enum { MAX_DISKS = 16 };
		struct {
			Block::Connection          *blk_con;
			Block::Session::Operations  ops;
			Genode::size_t              blk_size;
			Genode::size_t              blk_cnt;
		} _diskcon[MAX_DISKS];

		Genode::Lock        _startup_lock;
		Synced_motherboard &_motherboard;
		char               *_backing_store_base;
		char               *_backing_store_fb_base;

	public:

		/**
		 * Constructor
		 */
		Vancouver_disk(Synced_motherboard &,
		               char * backing_store_base,
		               char * backing_store_fb_base);

		~Vancouver_disk();

		void entry();

		bool receive(MessageDisk &msg);

		void register_host_operations(Motherboard &);
};

#endif /* _DISK_H_ */
