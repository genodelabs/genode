/*
 * \brief  USB storage glue
 * \author Sebastian Sumpf <sebastian.sumpf@genode-labs.com>
 * \date   2012-05-06
 */

/*
 * Copyright (C) 2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#include <base/rpc_server.h>
#include <block_session/block_session.h>
#include <cap_session/connection.h>
#include <util/endian.h>
#include <util/list.h>

#include <lx_emul.h>

#include <storage/component.h>
#include <storage/scsi.h>
#include "signal.h"

static Signal_helper *_signal = 0;

class Storage_device : public Genode::List<Storage_device>::Element,
                       public Block::Device
{
	private:

		Genode::size_t      _block_size;
		Block::sector_t     _block_count;
		struct scsi_device *_sdev;

		static void _sync_done(struct scsi_cmnd *cmnd) {
			complete((struct completion *)cmnd->back); }

		static void _async_done(struct scsi_cmnd *cmnd)
		{
			Block::Session_component *session = static_cast<Block::Session_component *>(cmnd->session);
			Block::Packet_descriptor *packet  = static_cast<Block::Packet_descriptor *>(cmnd->packet);

			if (verbose)
				PDBG("ACK packet for block: %zu status: %d", packet->block_number(), cmnd->result);

			session->complete(*packet, true);
			_scsi_free_command(cmnd);
		}

		void _capacity()
		{
			struct completion comp;

			struct scsi_cmnd *cmnd = _scsi_alloc_command();

			/* alloc data for command */
			scsi_alloc_buffer(8, cmnd);

			cmnd->cmnd[0]           = READ_CAPACITY;
			cmnd->cmd_len           = 10;
			cmnd->device            = _sdev;
			cmnd->sc_data_direction = DMA_FROM_DEVICE;

			init_completion(&comp);
			cmnd->back = &comp;
			cmnd->scsi_done = _sync_done;

			_sdev->host->hostt->queuecommand(_sdev->host, cmnd);
			wait_for_completion(&comp);

			Genode::uint32_t *data = (Genode::uint32_t *)scsi_buffer_data(cmnd);
			_block_count = bswap(data[0]);
			_block_size  = bswap(data[1]);
		
			/* if device returns the highest block number */
			if (!_sdev->fix_capacity)
				_block_count++;
		
			if (verbose)
				PDBG("block size: %zu block count: %llu", _block_size, _block_count);
		
			scsi_free_buffer(cmnd);
			_scsi_free_command(cmnd);
		}


		Storage_device(struct scsi_device *sdev) : _sdev(sdev) 
		{
			/* read device capacity */
			_capacity();
		}

	public:

		static Storage_device *add(struct scsi_device *sdev) {
			return new (Genode::env()->heap()) Storage_device(sdev); }

		Genode::size_t block_size() { return _block_size; }
		Genode::size_t block_count() { return _block_count; }

		void io(Block::Session_component *session, Block::Packet_descriptor &packet,
		        Genode::addr_t virt, Genode::addr_t phys)
		{
			Block::sector_t  block_nr    = packet.block_number();
			Genode::uint16_t block_count = packet.block_count() & 0xffff;
			bool read = packet.operation() == Block::Packet_descriptor::WRITE ? false : true;

			if (block_nr > _block_count)
				throw -1;
		
			if (verbose)
				PDBG("PACKET: phys: %lx block: %llu count: %u %s",
				     phys, block_nr, block_count, read ? "read" : "write");

			struct scsi_cmnd *cmnd = _scsi_alloc_command();

			cmnd->cmnd[0]           = read ? READ_10 : WRITE_10;
			cmnd->cmd_len           = 10;
			cmnd->device            = _sdev;
			cmnd->sc_data_direction = read ? DMA_FROM_DEVICE : DMA_TO_DEVICE;
			cmnd->scsi_done         = _async_done;

			Block::Packet_descriptor *p = new (Genode::env()->heap()) Block::Packet_descriptor();
			*p = packet;
			cmnd->packet  = (void *)p;
			cmnd->session = (void *)session;

			Genode::uint32_t be_block_nr = bswap<Genode::uint32_t>(block_nr);
			memcpy(&cmnd->cmnd[2], &be_block_nr, 4);

			/* transfer one block */
			Genode::uint16_t be_block_count =  bswap(block_count);
			memcpy(&cmnd->cmnd[7], &be_block_count, 2);

			/* setup command */
			scsi_setup_buffer(cmnd, block_count * _block_size, (void *)virt, phys);

			/*
			 * Required by 'last_sector_hacks' in 'drivers/usb/storage/transprot.c
			 */
			struct request req;
			req.rq_disk = 0;
			cmnd->request = &req;

			/* send command to host driver */
			if (_sdev->host->hostt->queuecommand(_sdev->host, cmnd)) {
				throw -2;
			}
		}
};


void Storage::init(Genode::Signal_receiver *recv) {
	_signal = new (Genode::env()->heap()) Signal_helper(recv); }


void scsi_add_device(struct scsi_device *sdev)
{
	using namespace Genode;
	static bool announce = false;

	Storage_device *device = Storage_device::add(sdev);

	/*
	 * XXX  move to 'main'
	 */
	if (!announce) {
		static Cap_connection cap_stor;
		static Rpc_entrypoint ep_stor(&cap_stor, 4096, "usb_stor_ep");
		static Block::Root root(&ep_stor, env()->heap(), _signal->receiver(), device);
		env()->parent()->announce(ep_stor.manage(&root));
		announce = true;
	}
}

