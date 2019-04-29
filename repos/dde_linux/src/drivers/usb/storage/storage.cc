/*
 * \brief  USB storage glue
 * \author Sebastian Sumpf <sebastian.sumpf@genode-labs.com>
 * \date   2012-05-06
 */

/*
 * Copyright (C) 2012-2017 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

#include <base/log.h>
#include <base/rpc_server.h>
#include <block/component.h>
#include <util/endian.h>
#include <util/list.h>

#include <lx_emul.h>

#include <lx_kit/malloc.h>
#include <lx_kit/backend_alloc.h>
#include <lx_kit/scheduler.h>

#include <lx_emul/extern_c_begin.h>
#include <storage/scsi.h>
#include <drivers/usb/storage/usb.h>
#include <lx_emul/extern_c_end.h>

#include "signal.h"

static Signal_helper *_signal = 0;

class Storage_device : public Genode::List<Storage_device>::Element,
                       public Block::Driver
{
	private:

		Genode::size_t      _block_size;
		Block::sector_t     _block_count;
		struct scsi_device *_sdev;

		static void _sync_done(struct scsi_cmnd *cmnd) {
			complete((struct completion *)cmnd->back); }

		static void _async_done(struct scsi_cmnd *cmnd);

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
			_block_count = host_to_big_endian(data[0]);
			_block_size  = host_to_big_endian(data[1]);

			/* if device returns the highest block number */
			if (!_sdev->fix_capacity)
				_block_count++;

			if (verbose)
				Genode::log("block size: ", _block_size, " block count", _block_count);

			scsi_free_buffer(cmnd);
			_scsi_free_command(cmnd);
		}

		void _io(Block::sector_t block_nr, Genode::size_t block_count,
		         Block::Packet_descriptor packet,
		         Genode::addr_t phys, bool read)
		{
			if (block_nr > _block_count)
				throw Io_error();

			if (verbose)
				log("PACKET: phys: ", Genode::Hex(phys), " block: ", block_nr, "count: ", block_count,
				    read ? " read" : " write");

			/* check if we can call queuecommand */
			struct us_data *us = (struct us_data *) _sdev->host->hostdata;
			if (us->srb != NULL)
				throw Request_congestion();

			struct scsi_cmnd *cmnd = _scsi_alloc_command();

			cmnd->cmnd[0]           = read ? READ_10 : WRITE_10;
			cmnd->cmd_len           = 10;
			cmnd->device            = _sdev;
			cmnd->sc_data_direction = read ? DMA_FROM_DEVICE : DMA_TO_DEVICE;
			cmnd->scsi_done         = _async_done;

			Block::Packet_descriptor *p = new (Lx::Malloc::mem()) Block::Packet_descriptor();
			*p = packet;
			cmnd->packet  = (void *)p;

			Genode::uint32_t be_block_nr = host_to_big_endian<Genode::uint32_t>(block_nr);
			memcpy(&cmnd->cmnd[2], &be_block_nr, 4);

			/* transfer one block */
			Genode::uint16_t be_block_count = host_to_big_endian<Genode::uint16_t>(block_count);
			memcpy(&cmnd->cmnd[7], &be_block_count, 2);

			/* setup command */
			scsi_setup_buffer(cmnd, block_count * _block_size, (void *)0, phys);

			/*
			 * Required by 'last_sector_hacks' in 'drivers/usb/storage/transprot.c
			 */
			struct request req;
			req.rq_disk = 0;
			cmnd->request = &req;

			/* send command to host driver */
			_sdev->host->hostt->queuecommand(_sdev->host, cmnd);

			/* schedule next task if we come from EP */
			if (Lx::scheduler().active() == false)
				Lx::scheduler().schedule();
		}

	public:

		Storage_device(Genode::Ram_allocator &ram, struct scsi_device *sdev)
		: Block::Driver(ram), _sdev(sdev)
		{
			/* read device capacity */
			_capacity();
		}

		Block::Session::Info info() const override
		{
			return { .block_size  = _block_size,
			         .block_count = _block_count,
			         .align_log2  = Genode::log2(_block_size),
			         .writeable   = true };
		}

		void read_dma(Block::sector_t           block_number,
		              Genode::size_t            block_count,
		              Genode::addr_t            phys,
		              Block::Packet_descriptor &packet) {
			_io(block_number, block_count, packet, phys, true); }

		void write_dma(Block::sector_t           block_number,
		               Genode::size_t            block_count,
		               Genode::addr_t            phys,
		               Block::Packet_descriptor &packet) {
			_io(block_number, block_count, packet, phys, false); }

		bool dma_enabled() { return true; }

		Genode::Ram_dataspace_capability alloc_dma_buffer(Genode::size_t size) {
			return Lx::backend_alloc(size, Genode::UNCACHED); }

		void free_dma_buffer(Genode::Ram_dataspace_capability cap) {
			return Lx::backend_free(cap); }
};


void Storage::init(Genode::Env &env) {
	_signal = new (Lx::Malloc::mem()) Signal_helper(env); }


struct Factory : Block::Driver_factory
{
	Storage_device device;

	Factory(Genode::Ram_allocator &ram, struct scsi_device *sdev)
	: device(ram, sdev) {}

	Block::Driver *create() { return &device; }
	void destroy(Block::Driver *driver) { }
};


static Storage_device *device = nullptr;
static work_struct delayed;


extern "C" void ack_packet(work_struct *work)
{
	Block::Packet_descriptor *packet =
		(Block::Packet_descriptor *)(work->data);

	if (verbose)
		Genode::log("ACK packet for block: ", packet->block_number());

	device->ack_packet(*packet);
	Genode::destroy(Lx::Malloc::mem(), packet);
}


void scsi_add_device(struct scsi_device *sdev)
{
	using namespace Genode;
	static bool announce = false;

	static struct Factory factory(_signal->ram(), sdev);
	device = &factory.device;

	/*
	 * XXX  move to 'main'
	 */
	if (!announce) {
		enum { WRITEABLE = true };

		PREPARE_WORK(&delayed, ack_packet);
		static Block::Root root(_signal->ep(), Lx::Malloc::mem(),
		                        _signal->rm(), factory, WRITEABLE);
		_signal->parent().announce(_signal->ep().rpc_ep().manage(&root));
		announce = true;
	}
}


void Storage_device::_async_done(struct scsi_cmnd *cmnd)
{
	/*
	 * Schedule packet ack, because we are called here in USB storage thread
	 * context, the from code that will clear the command queue later, so we
	 * cannot send the next packet from here
	 */
	delayed.data = cmnd->packet;
	schedule_work(&delayed);

	scsi_free_buffer(cmnd);
	_scsi_free_command(cmnd);

}
