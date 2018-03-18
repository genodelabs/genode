/*
 * \brief  Block interface
 * \author Markus Partheymueller
 * \author Alexander Boettcher
 * \date   2012-09-15
 */

/*
 * Copyright (C) 2012 Intel Corporation
 * Copyright (C) 2013-2018 Genode Labs GmbH
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
		char * const        source_addr = source->packet_content(packet);

		/* find the corresponding MessageDisk object */
		Avl_entry * obj = lookup_and_remove(_lookup_msg, source_addr);
		if (!obj) {
			Genode::warning("unknown MessageDisk object - drop ack of block session ", (void *)source_addr);
			continue;
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

				bool const ok = check_dma_descriptors(msg,
					[&](char * const dma_addr, unsigned i)
					{
						size_t const bytecount = msg->dma[i].bytecount;

						if (bytecount          > packet.size() ||
						    sector             > packet.size() ||
						    sector + bytecount > packet.size() ||
						    source_addr > source->packet_content(packet) + packet.size() - sector - bytecount ||
						    _backing_store_base + _backing_store_size - bytecount < dma_addr)
							return false;

						memcpy(dma_addr, source_addr + sector, bytecount);
						sector += bytecount;
						return true;
					});

				if (!ok)
					Genode::warning("DMA bounds violation during read");

				destroy(disk_heap(), msg->dma);
				msg->dma = nullptr;
			}

			MessageDiskCommit mdc (disknr, msg->usertag, MessageDisk::DISK_OK);
			_motherboard()->bus_diskcommit.send(mdc);
		}
 
		{
			Genode::Lock::Guard lock_guard(_alloc_lock);
			source->release_packet(packet);
		}
		destroy(&_tslab_msg, msg);
	}

	/* restart disk operations suspended due to out of memory by alloc_packet */
	check_restart();
}


bool Seoul::Disk::receive(MessageDisk &msg)
{
	static Vmm::Utcb_guard::Utcb_backup utcb_backup;
	Vmm::Utcb_guard guard(utcb_backup);

	if (msg.disknr >= MAX_DISKS)
		Logging::panic("You configured more disks than supported.\n");

	struct disk_session &disk = _diskcon[msg.disknr];

	if (!disk.blk_size) {
		Genode::String<16> label("VirtualDisk ", msg.disknr);
		/*
		 * If we receive a message for this disk the first time, create the
		 * structure for it.
		 */
		try {
			Genode::Allocator_avl * block_alloc =
				new (disk_heap()) Genode::Allocator_avl(disk_heap());

			disk.blk_con =
				new (disk_heap()) Block::Connection(_env, block_alloc,
				                                    4*512*1024,
				                                    label.string());
			disk.signal =
				new (disk_heap()) Seoul::Disk_signal(_env.ep(), *this,
				                                     *disk.blk_con, msg.disknr);
		} catch (...) {
			/* there is none. */
			return false;
		}

		disk.blk_con->info(&disk.blk_cnt, &disk.blk_size, &disk.ops);
	}

	msg.error = MessageDisk::DISK_OK;

	switch (msg.type) {
	case MessageDisk::DISK_GET_PARAMS:
	{
		Genode::String<16> label("VirtualDisk ", msg.disknr);

		msg.params->flags = DiskParameter::FLAG_HARDDISK;
		msg.params->sectors = disk.blk_cnt;
		msg.params->sectorsize = disk.blk_size;
		msg.params->maxrequestcount = disk.blk_cnt;
		memcpy(msg.params->name, label.string(), label.length());

		return true;
	}
	case MessageDisk::DISK_WRITE:
		/* don't write on read only medium */
		if (!disk.ops.supported(Block::Packet_descriptor::WRITE)) {
			MessageDiskCommit ro(msg.disknr, msg.usertag,
		                     MessageDisk::DISK_STATUS_DEVICE);
			_motherboard()->bus_diskcommit.send(ro);
			return true;
		}
	case MessageDisk::DISK_READ:
		/* read and write handling */
		return execute(msg.type == MessageDisk::DISK_WRITE, disk, msg);
	default:
		Logging::printf("Got MessageDisk type %x\n", msg.type);
		return false;
	}
}

void Seoul::Disk::check_restart()
{
	bool restarted = true;

	while (restarted) {
		Avl_entry * obj = lookup_and_remove(_restart_msg);
		if (!obj)
			return;

		MessageDisk       * const   msg = obj->msg();
		struct disk_session const &disk = _diskcon[msg->disknr];

		restarted = restart(disk, msg);

		if (restarted) {
			destroy(&_tslab_avl, obj);
		} else {
			Genode::Lock::Guard lock_guard(_alloc_lock);
			_restart_msg.insert(obj);
		}
	}
}

bool Seoul::Disk::restart(struct disk_session const &disk,
                          MessageDisk * const msg)
{
	Block::Session::Tx::Source * const source = disk.blk_con->tx();

	unsigned long const  total = DmaDescriptor::sum_length(msg->dmacount, msg->dma);
	unsigned const blk_size    = disk.blk_size;
	unsigned long const blocks = total/blk_size + ((total%blk_size) ? 1 : 0);
	bool          const write  = msg->type == MessageDisk::DISK_WRITE;

	Block::Packet_descriptor packet;

	try {
		Genode::Lock::Guard lock_guard(_alloc_lock);

		packet = Block::Packet_descriptor(
			source->alloc_packet(blocks * blk_size),
			(write) ? Block::Packet_descriptor::WRITE
			        : Block::Packet_descriptor::READ,
			msg->sector, blocks);

		char * source_addr = source->packet_content(packet);
		_lookup_msg.insert(new (&_tslab_avl) Avl_entry(source_addr, msg));

	} catch (...) { return false; }

	/* read */
	if (!write) {
		source->submit_packet(packet);
		return true;
	}

	/* write */
	char * source_addr = source->packet_content(packet);
	source_addr += (msg->sector - packet.block_number()) * blk_size;

	for (unsigned i = 0; i < msg->dmacount; i++) {
		char * const dma_addr = _backing_store_base +
		                        msg->dma[i].byteoffset +
		                        msg->physoffset;

		memcpy(source_addr, dma_addr, msg->dma[i].bytecount);
		source_addr += msg->dma[i].bytecount;
	}

	/* free up DMA descriptors of write */
	destroy(disk_heap(), msg->dma);
	msg->dma = nullptr;

	source->submit_packet(packet);
	return true;
}


bool Seoul::Disk::execute(bool const write, struct disk_session const &disk,
                          MessageDisk const &msg)
{
	unsigned long long const sector  = msg.sector;
	unsigned long const  total   = DmaDescriptor::sum_length(msg.dmacount, msg.dma);
	unsigned long const blk_size = disk.blk_size;
	unsigned long const blocks   = total/blk_size + ((total%blk_size) ? 1 : 0);

	Block::Session::Tx::Source * const source = disk.blk_con->tx();
	Block::Packet_descriptor packet;

	/* msg copy required for acknowledgements */
	MessageDisk * const msg_cpy = new (&_tslab_msg) MessageDisk(msg);
	char * source_addr          = nullptr;

	try {
		Genode::Lock::Guard lock_guard(_alloc_lock);

		packet = Block::Packet_descriptor(
			source->alloc_packet(blocks * blk_size),
			(write) ? Block::Packet_descriptor::WRITE
			        : Block::Packet_descriptor::READ,
			sector, blocks);

		source_addr = source->packet_content(packet);

		_lookup_msg.insert(new (&_tslab_avl) Avl_entry(source_addr, msg_cpy));
	} catch (Block::Session::Tx::Source::Packet_alloc_failed) {
		/* msg_cpy object will be used/freed below by copy_dma_descriptors */
	} catch (...) {
		if (msg_cpy)
			destroy(&_tslab_msg, msg_cpy);

		Logging::printf("could not allocate disk block elements - "
		                "write=%u blocks=%lu\n", write, blocks);
		return false;
	}

	/*
	 * Copy DMA descriptors ever for read requests and for deferred write
	 * requests. The descriptors can be changed by the guest at any time.
	 */
	bool const copy_dma_descriptors = !source_addr || !write;

	/* invalid packet allocation or read request */
	if (copy_dma_descriptors) {
		msg_cpy->dma = new (disk_heap()) DmaDescriptor[msg_cpy->dmacount];
		for (unsigned i = 0; i < msg_cpy->dmacount; i++)
			memcpy(msg_cpy->dma + i, msg.dma + i, sizeof(DmaDescriptor));

		/* validate DMA descriptors */
		bool const ok = check_dma_descriptors(msg_cpy,
			[&](char * const dma_addr, unsigned i)
			{
				if (!write)
					/* evaluated during receive */
					return true;

				size_t const bytecount = msg_cpy->dma[i].bytecount;

				if (bytecount > packet.size() ||
				    source_addr > source->packet_content(packet) + packet.size() - bytecount ||
				    _backing_store_base + _backing_store_size - bytecount < dma_addr)
					return false;

				return true;
			});

		if (ok) {
			/* DMA descriptors look good - go ahead with disk request */

			if (source_addr)
				/* valid packet for read request */
				source->submit_packet(packet);
			else {
				/* failed packet allocation, restart at later point in time */
				Genode::Lock::Guard lock_guard(_alloc_lock);
				_restart_msg.insert(new (&_tslab_avl) Avl_entry(source_addr,
				                                                msg_cpy));
			}
			return true;
		}

		/* DMA descriptors look bad - free resources */
		destroy(disk_heap(), msg_cpy->dma);
		destroy(&_tslab_msg, msg_cpy);
		if (source_addr) {
			Genode::Lock::Guard lock_guard(_alloc_lock);
			source->release_packet(packet);
		}
		return false;
	}

	/* valid packet allocation for write request */
	source_addr += (sector - packet.block_number()) * blk_size;

	bool const ok = check_dma_descriptors(msg_cpy,
		[&](char * const dma_addr, unsigned i)
	{
		/* read bytecount from guest memory once and don't evaluate again */
		size_t const bytecount = msg.dma[i].bytecount;

		if (bytecount > packet.size() ||
		    source_addr > source->packet_content(packet) + packet.size() - bytecount ||
		    _backing_store_base + _backing_store_size - bytecount < dma_addr)
			return false;

		memcpy(source_addr, dma_addr, bytecount);
		source_addr += bytecount;

		return true;
	});

	if (ok) {
		/* don't needed anymore + protect us to use it again */
		msg_cpy->dma = nullptr;
		source->submit_packet(packet);
	} else {
		destroy(&_tslab_msg, msg_cpy);

		Genode::Lock::Guard lock_guard(_alloc_lock);
		source->release_packet(packet);
	}
	return ok;
}
