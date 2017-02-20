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

/* Genode includes */
#include <base/log.h>
#include <block_session/connection.h>
#include <base/heap.h>

/* VMM utility includes */
#include <vmm/utcb_guard.h>

/* local includes */
#include "disk.h"

/* Seoul includes */
#include <host/dma.h>

static Genode::Heap * disk_heap(Genode::Ram_session *ram = nullptr,
                                Genode::Region_map *rm = nullptr)
{
	static Genode::Heap heap(ram, rm);
	return &heap;
}
static Genode::Heap * disk_heap_msg(Genode::Env &env)
{
	static Genode::Heap heap(&env.ram(), &env.rm(), 4096);
	return &heap;
}
static Genode::Heap * disk_heap_avl(Genode::Env &env)
{
	static Genode::Heap heap(&env.ram(), &env.rm(), 4096);
	return &heap;
}


Seoul::Disk::Disk(Genode::Env &env, Synced_motherboard &mb,
                  char * backing_store_base, Genode::size_t backing_store_size)
:
	_env(env),
	_motherboard(mb),
	_backing_store_base(backing_store_base),
	_backing_store_size(backing_store_size),
	_tslab_msg(disk_heap_msg(env)),
	_tslab_avl(disk_heap_avl(env))
{
	/* initialize disk heap */
	disk_heap(&env.ram(), &env.rm());

	/* initialize struct with 0 size */
	for (int i=0; i < MAX_DISKS; i++) {
		_diskcon[i].blk_size = 0;
	}
}


void Seoul::Disk::register_host_operations(Motherboard &motherboard)
{
	motherboard.bus_disk.add(this, receive_static<MessageDisk>);
}

void Seoul::Disk_signal::_signal() { _obj.handle_disk(_id); }

void Seoul::Disk::handle_disk(unsigned disknr)
{
	Block::Session::Tx::Source *source = _diskcon[disknr].blk_con->tx();

	while (source->ack_avail())
	{
		Block::Packet_descriptor packet = source->get_acked_packet();
		char * source_addr              = source->packet_content(packet);

		/* find the corresponding MessageDisk object */
		Avl_entry * obj = 0;
		{
			Genode::Lock::Guard lock_guard(_lookup_msg_lock);
			obj = _lookup_msg.first();
			if (obj)
				obj = obj->find(reinterpret_cast<Genode::addr_t>(source_addr));

			if (!obj) {
				Genode::warning("unknown MessageDisk object - drop ack of block session");
				continue;
			}

			/* remove helper object */
			_lookup_msg.remove(obj);
		}
		/* got the MessageDisk object */
		MessageDisk * msg = obj->msg();
		/* delete helper object */
		destroy(&_tslab_avl, obj);

		/* go ahead and tell VMM about new block event */
		if (!packet.succeeded() || 
		    !(packet.operation() == Block::Packet_descriptor::Opcode::READ ||
		      packet.operation() == Block::Packet_descriptor::Opcode::WRITE)) {

			Genode::warning("getting block failed");

			MessageDiskCommit mdc(disknr, msg->usertag,
			                      MessageDisk::DISK_STATUS_DEVICE);
			_motherboard()->bus_diskcommit.send(mdc);
	
		} else {

			if (packet.operation() == Block::Packet_descriptor::Opcode::READ) {
						
				unsigned long long sector = msg->sector;
				sector = (sector-packet.block_number()) * _diskcon[disknr].blk_size;

				for (unsigned i = 0; i < msg->dmacount; i++) {
					char * dma_addr = _backing_store_base +
					                  msg->dma[i].byteoffset + msg->physoffset;

					// check for bounds
					if (dma_addr >= _backing_store_base + _backing_store_size
					 || dma_addr < _backing_store_base) {
						Genode::error("dma bounds violation");
					} else
						memcpy(dma_addr, source_addr + sector,
						       msg->dma[i].bytecount);

					sector += msg->dma[i].bytecount;
				}

				destroy(disk_heap(), msg->dma);
				msg->dma = 0;
			}

			MessageDiskCommit mdc (disknr, msg->usertag, MessageDisk::DISK_OK);
			_motherboard()->bus_diskcommit.send(mdc);
		}
 
		source->release_packet(packet);
		destroy(&_tslab_msg, msg);
	}
}


bool Seoul::Disk::receive(MessageDisk &msg)
{
	static Vmm::Utcb_guard::Utcb_backup utcb_backup;
	Vmm::Utcb_guard guard(utcb_backup);

	if (msg.disknr >= MAX_DISKS)
		Logging::panic("You configured more disks than supported.\n");

	/*
	 * If we receive a message for this disk the first time, create the
	 * structure for it.
	 */
	Genode::String<16> label("VirtualDisk ", msg.disknr);

	if (!_diskcon[msg.disknr].blk_size) {
		try {
			Genode::Allocator_avl * block_alloc =
				new (disk_heap()) Genode::Allocator_avl(disk_heap());

			_diskcon[msg.disknr].blk_con =
				new (disk_heap()) Block::Connection(_env, block_alloc,
				                                    4*512*1024,
				                                    label.string());
			_diskcon[msg.disknr].signal =
				new (disk_heap()) Seoul::Disk_signal(_env.ep(), *this,
				                                     msg.disknr);

			_diskcon[msg.disknr].blk_con->tx_channel()->sigh_ack_avail(
				_diskcon[msg.disknr].signal->sigh);
		} catch (...) {
			/* there is none. */
			return false;
		}

		_diskcon[msg.disknr].blk_con->info(&_diskcon[msg.disknr].blk_cnt,
		                                   &_diskcon[msg.disknr].blk_size,
		                                   &_diskcon[msg.disknr].ops);

		Logging::printf("Got info: %llu blocks (%lu B), ops (R: %x, W:%x)\n ",
		        _diskcon[msg.disknr].blk_cnt,
		        _diskcon[msg.disknr].blk_size,
		        _diskcon[msg.disknr].ops.supported(Block::Packet_descriptor::READ),
		        _diskcon[msg.disknr].ops.supported(Block::Packet_descriptor::WRITE)
		);
	}

	msg.error = MessageDisk::DISK_OK;

	switch (msg.type) {
	case MessageDisk::DISK_GET_PARAMS:
		{
			msg.params->flags = DiskParameter::FLAG_HARDDISK;
			msg.params->sectors = _diskcon[msg.disknr].blk_cnt;
			msg.params->sectorsize = _diskcon[msg.disknr].blk_size;
			msg.params->maxrequestcount = _diskcon[msg.disknr].blk_cnt;
			memcpy(msg.params->name, label.string(), label.length());
		}
		return true;
	case MessageDisk::DISK_READ:
	case MessageDisk::DISK_WRITE:
		{
			bool write = (msg.type == MessageDisk::DISK_WRITE);

			if (write && !_diskcon[msg.disknr].ops.supported(Block::Packet_descriptor::WRITE)) {
				MessageDiskCommit ro(msg.disknr, msg.usertag,
				                     MessageDisk::DISK_STATUS_DEVICE);
				_motherboard()->bus_diskcommit.send(ro);
				return true;
			}

			unsigned long long sector = msg.sector;
			unsigned long total = DmaDescriptor::sum_length(msg.dmacount, msg.dma);
			unsigned long blocks = total/_diskcon[msg.disknr].blk_size;
			unsigned const blk_size = _diskcon[msg.disknr].blk_size;

			if (blocks * blk_size < total) blocks++;

			Block::Session::Tx::Source *source = _diskcon[msg.disknr].blk_con->tx();
			Block::Packet_descriptor packet;

			/* save original msg, required when we get acknowledgements */
			MessageDisk * msg_cpy = 0;
			try {
				msg_cpy = new (&_tslab_msg) MessageDisk(msg);

				packet = Block::Packet_descriptor(
					source->alloc_packet(blocks * blk_size),
					(write) ? Block::Packet_descriptor::WRITE
					        : Block::Packet_descriptor::READ,
					sector, blocks);
			} catch (...) {
				if (msg_cpy)
					destroy(&_tslab_msg, msg_cpy);
				Logging::printf("could not allocate disk block elements\n");
				return false;
			}

			char * source_addr = source->packet_content(packet);
			{
				Genode::Lock::Guard lock_guard(_lookup_msg_lock);
				_lookup_msg.insert(new (&_tslab_avl) Avl_entry(source_addr,
				                                               msg_cpy));
			}

			/* copy DMA descriptors for read requests - they may change */
			if (!write) {
				msg_cpy->dma = new (disk_heap()) DmaDescriptor[msg_cpy->dmacount];
				for (unsigned i = 0; i < msg_cpy->dmacount; i++)
					memcpy(msg_cpy->dma + i, msg.dma + i, sizeof(DmaDescriptor));
			}


			/* for write operation */	
			source_addr       += (sector - packet.block_number()) * blk_size;

			/* check bounds for read and write operations */	
			for (unsigned i = 0; i < msg_cpy->dmacount; i++) {
				char * dma_addr = _backing_store_base + msg_cpy->dma[i].byteoffset
				                  + msg_cpy->physoffset;

				/* check for bounds */
				if (dma_addr >= _backing_store_base + _backing_store_size
				 || dma_addr < _backing_store_base) { 
					/* drop allocated objects not needed in error case */
					if (write)
						destroy(disk_heap(), msg_cpy->dma);
					destroy(&_tslab_msg, msg_cpy);
					source->release_packet(packet);
					return false;
				}

				if (write) {
					memcpy(source_addr, dma_addr, msg.dma[i].bytecount);
					source_addr += msg.dma[i].bytecount;
				}
			}

			source->submit_packet(packet);
		}
		break;
	default:
		Logging::printf("Got MessageDisk type %x\n", msg.type);
		return false;
	}
	return true;
}


Seoul::Disk::~Disk()
{
	/* XXX: Close all connections */
}
