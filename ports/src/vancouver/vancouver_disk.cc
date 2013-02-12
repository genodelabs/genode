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

#include <base/allocator_avl.h>
#include <base/printf.h>
#include <base/sleep.h>
#include <base/thread.h>
#include <block_session/connection.h>
#include <util/string.h>

#include <vancouver_disk.h>

static Genode::Native_utcb utcb_backup;

extern Genode::Lock timeouts_lock;
extern bool disk_init;

Vancouver_disk::Vancouver_disk(Motherboard &mb,
                               char * backing_store_base,
                               char * backing_store_fb_base)
: _mb(mb), _backing_store_base(backing_store_base), _backing_store_fb_base(backing_store_fb_base) {
	/* Initialize struct with 0 size */
	for (int i=0; i < MAX_DISKS; i++) {
		_diskcon[i].blk_size = 0;
	}
	start();
}

void Vancouver_disk::entry()
{
	Logging::printf("Hello, this is Vancouver_disk.\n");

	/* Attach to Disk Bus */
	_mb.bus_disk.add(this, receive_static<MessageDisk>);

	disk_init = true;
}

bool Vancouver_disk::receive(MessageDisk &msg)
{
	utcb_backup = *Genode::Thread_base::myself()->utcb();

	if (msg.disknr >= MAX_DISKS)
		Logging::panic("You configured more disks than supported.\n");

	/* If we receive a message for this disk the first time,
	 * create the structure for it */
	char label[14];
	Genode::snprintf(label, 14, "VirtualDisk %2u", msg.disknr);

	if (!_diskcon[msg.disknr].blk_size) {

		Genode::Allocator_avl * block_alloc = new Genode::Allocator_avl(Genode::env()->heap());
		try {
			_diskcon[msg.disknr].blk_con = new Block::Connection(block_alloc, 4*512*1024, label);
		} catch (...) {
			/* There is none. */
			return false;
		}

		_diskcon[msg.disknr].blk_con->info(
			&_diskcon[msg.disknr].blk_cnt,
			&_diskcon[msg.disknr].blk_size,
			&_diskcon[msg.disknr].ops);

		Logging::printf("Got info: %lu blocks (%u B), ops (R: %x, W:%x)\n ",
		        _diskcon[msg.disknr].blk_cnt,
		        _diskcon[msg.disknr].blk_size,
		        _diskcon[msg.disknr].ops.supported(Block::Packet_descriptor::READ),
		        _diskcon[msg.disknr].ops.supported(Block::Packet_descriptor::WRITE)
		);
	}


	Block::Session::Tx::Source *source = _diskcon[msg.disknr].blk_con->tx();

	switch (msg.type) {
	case MessageDisk::DISK_GET_PARAMS:
		{
			msg.error = MessageDisk::DISK_OK;
			msg.params->flags = DiskParameter::FLAG_HARDDISK;
			msg.params->sectors = _diskcon[msg.disknr].blk_cnt;
			msg.params->sectorsize = _diskcon[msg.disknr].blk_size;
			msg.params->maxrequestcount = _diskcon[msg.disknr].blk_cnt;
			memcpy(msg.params->name, label, strlen(label));
			msg.params->name[strlen(label)] = 0;
		}
		return true;
	case MessageDisk::DISK_READ:
	case MessageDisk::DISK_WRITE:
		{
			bool read = (msg.type == MessageDisk::DISK_READ);

			if (!read && !_diskcon[msg.disknr].ops.supported(Block::Packet_descriptor::WRITE)) {
				MessageDiskCommit ro(msg.disknr, msg.usertag,
				                     MessageDisk::DISK_STATUS_DEVICE);
				_mb.bus_diskcommit.send(ro);
				*Genode::Thread_base::myself()->utcb() = utcb_backup;
				return true;
			}

			unsigned long long sector = msg.sector;
			unsigned long total = DmaDescriptor::sum_length(msg.dmacount, msg.dma);
			unsigned long blocks = total/_diskcon[msg.disknr].blk_size;

			if (blocks*_diskcon[msg.disknr].blk_size < total)
				blocks++;

			Block::Session::Tx::Source *source = _diskcon[msg.disknr].blk_con->tx();
			Block::Packet_descriptor p(source->alloc_packet(blocks*_diskcon[msg.disknr].blk_size),
					   (read) ? Block::Packet_descriptor::READ
						  : Block::Packet_descriptor::WRITE,
			                   sector,
			                   blocks);

			if (read) {
				source->submit_packet(p);
				p = source->get_acked_packet();
			}

			if (!read || p.succeeded()) {
				char * source_addr = source->packet_content(p);

				sector = (sector-p.block_number())*_diskcon[msg.disknr].blk_size;

				for (int i=0; i< msg.dmacount; i++) {
					char * dma_addr = (char *) (msg.dma[i].byteoffset + msg.physoffset +
				                                    _backing_store_base);

					/* Check for bounds */
					if (dma_addr >= _backing_store_fb_base ||
					    dma_addr < _backing_store_base)
						return false;

					if (read)
						memcpy(dma_addr, source_addr+sector, msg.dma[i].bytecount);
					else
						memcpy(source_addr+sector, dma_addr, msg.dma[i].bytecount);

					sector += msg.dma[i].bytecount;
				}

				if (!read) {
					source->submit_packet(p);
			        	p = source->get_acked_packet();
					if (!p.succeeded()) {
						Logging::printf("Operation failed.\n");
						{
							Genode::Lock::Guard guard(timeouts_lock);
							MessageDiskCommit commit(msg.disknr, msg.usertag,
							                         MessageDisk::DISK_STATUS_DEVICE);
							_mb.bus_diskcommit.send(commit);
							break;
						}
					}
				}

				{
					Genode::Lock::Guard guard(timeouts_lock);
					MessageDiskCommit commit(msg.disknr, msg.usertag,
					                         MessageDisk::DISK_OK);
					_mb.bus_diskcommit.send(commit);
				}
			} else {
				Logging::printf("Operation failed.\n");
				{
					Genode::Lock::Guard guard(timeouts_lock);
					MessageDiskCommit commit(msg.disknr, msg.usertag,
					                         MessageDisk::DISK_STATUS_DEVICE);
					_mb.bus_diskcommit.send(commit);
				}
			}

			source->release_packet(p);
		}
		break;
	default:
		Logging::printf("Got MessageDisk type %x\n", msg.type);
		*Genode::Thread_base::myself()->utcb() = utcb_backup;
		return false;
	}
	*Genode::Thread_base::myself()->utcb() = utcb_backup;
	return true;
}

Vancouver_disk::~Vancouver_disk()
{
	/* XXX: Close all connections */
}
