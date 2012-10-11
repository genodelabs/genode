/*
 * \brief  Block interface
 * \author Markus Partheymueller
 * \date   2012-09-15
 */

/*
 * Copyright (C) 2012 Intel Corporation    
 * 
 * This file is part of the Genode OS framework and being contributed under
 * the terms and conditions of the GNU General Public License version 2.   
 */

#ifndef _VANCOUVER_DISK_H
#define _VANCOUVER_DISK_H

/* Genode includes */
#include <base/allocator_avl.h>
#include <base/printf.h>
#include <base/sleep.h>
#include <base/thread.h>
#include <block_session/connection.h>
#include <util/string.h>

/* NOVA userland includes */
#include <nul/motherboard.h>
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

		Motherboard &_mb;
		char        *_backing_store_base;
		char        *_backing_store_fb_base;

	public:

		/**
		 * Constructor
		 */
		Vancouver_disk(Motherboard &mb, char * backing_store_base, char * backing_store_fb_base);
		~Vancouver_disk();

		void entry();

		bool receive(MessageDisk &msg);
};

#endif
