/*
 * \brief  ATA device class
 * \author Sebastian.Sump <Sebastian.Sumpf@genode-labs.com>
 * \date   2010-07-15
 */

/*
 * Copyright (C) 2010-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <base/lock.h>
#include <io_port_session/io_port_session.h>
#include <irq_session/connection.h>

/* local includes */
#include "ata_device.h"
#include "ata_bus_master.h"
#include "io.h"
#include "mindrvr.h"

static const int verbose = 0;

using namespace Genode;

Ata::Device::Device(unsigned base_cmd, unsigned base_ctrl)
: _lba48(false), _host_protected_area(false)
{
	_pio = new(env()->heap()) Ata::Io_port(base_cmd, base_ctrl);
}


Ata::Device::~Device()
{
	destroy(env()->heap(), _pio);

	if (_irq)
		destroy(env()->heap(), _irq);

	if (_bus_master)
		destroy(env()->heap(), _bus_master);

}


void Ata::Device::set_bus_master(bool secondary)
{
	_bus_master = new(env()->heap()) Ata::Bus_master(secondary);
}


void Ata::Device::set_irq(unsigned irq)
{
	_irq = new(env()->heap()) Irq_connection(irq);
}


void Ata::Device::probe_dma()
{
	/* find bus master interface base address */
	if (!_bus_master->scan_pci())
		return;

	/* read device information */
	reg_reset(dev_num());

	unsigned char *buffer;
	if (!(env()->heap()->alloc(4096, &buffer))) return;

	unsigned char cmd = dynamic_cast<Atapi_device *>(this) ? CMD_IDENTIFY_DEVICE_PACKET : CMD_IDENTIFY_DEVICE;
	if (!reg_pio_data_in_lba28(
	               dev_num(),
	               cmd,
	               0, 1,
	               0L,
	               buffer,
	               1L, 0 )) {

		/* check command set's LBA48 bit */
		_lba48 = !!(buffer[167] & 0x4);
		/* check for host protected area feature */
		_host_protected_area = !!(buffer[165] & 0x4);

		Genode::printf( "Adress mode is LBA%d\n", _lba48 ? 48 : 28 );
		Genode::printf("UDMA Modes supported:\n");

		for (int i = 0; i <= 5; i++) {
			Genode::printf("\t%d and below: %s enabled: %s\n",
			               i ,(buffer[176] & (1 << i)) ? "yes" : "no",
			                  (buffer[177] & (1 << i)) ? "yes" : "no");

			if (buffer[177] & (1 << i)) {

				/* initialize PRD's */
				dma_pci_config();
				_dma = true;
				return;
			}
		}
	}

	env()->heap()->free(buffer, 4096);
}


void Ata::Device::read_capacity()
{
	_block_start = 0;
	_block_size  = 512;

	enum { 
		CMD_NATIVE_MAX_ADDRESS     = 0xf8, /* LBA28 */
		CMD_NATIVE_MAX_ADDRESS_EXT = 0x27, /* LBA48 */
	};

	/*
	 * If both LBA48 is enabled and host protected area feature is set, then NATIVE MAX
	 * ADDRESS EXT becomes mandatory, use LBA28 otherwise
	 */
	if (_lba48 && _host_protected_area) {
		if (!reg_non_data_lba48(dev_num(), CMD_NATIVE_MAX_ADDRESS_EXT, 0, 1, 0UL, 0UL)) {

			_block_end  =  _pio->inb(CB_SN);              /* 0 - 7 */
			_block_end |=  _pio->inb(CB_CL) << 8;         /* 8 - 15 */
			_block_end |=  _pio->inb(CB_CH) << 16;        /* 16 - 23 */

			/* read higher order LBA registers */
			_pio->outb(CB_DC, CB_DC_HOB);
			_block_end |=  _pio->inb(CB_SN) << 24;        /* 24 - 31 */
			/* FIXME: read bits 32 - 47 */

		}
	} else {
		if (!reg_non_data_lba28(dev_num(), CMD_NATIVE_MAX_ADDRESS, 0, 1, 0UL)) {

			_block_end  =  _pio->inb(CB_SN);              /* 0 - 7 */
			_block_end |=  _pio->inb(CB_CL) << 8;         /* 8 - 15 */
			_block_end |=  _pio->inb(CB_CH) << 16;        /* 16 - 23 */
			_block_end |= (_pio->inb(CB_DH) & 0xf) << 24; /* 24 - 27 */
		}
	}

	PINF("First block: %u last block %u, block size %u",
	     _block_start, _block_end, _block_size);
}


void Ata::Device::_read(Genode::size_t  block_nr,
                        Genode::size_t  count,
                        char           *buffer,
                        bool            dma)
{
	Genode::size_t offset = 0;

	while (count > 0) {
		Genode::size_t c = count > 255 ? 255 : count;

		if (dma) {

			if (verbose)
				PDBG("DMA read: block %zu, c %zu, buffer: %p",
					 block_nr, c, (void*)(buffer + offset));

			if (!_lba48)
			{
				if (dma_pci_lba28(dev_num(), CMD_READ_DMA, 0, c, block_nr,
				                  (unsigned char*)(buffer + offset), c))
					throw Io_error();
			} else {
				if (dma_pci_lba48(dev_num(), CMD_READ_DMA_EXT, 0, c, 0, block_nr,
				                  (unsigned char*)(buffer + offset), c))
					throw Io_error();

			}
		}
		else {
			if (!_lba48)
			{
				if (reg_pio_data_in_lba28(dev_num(), CMD_READ_SECTORS, 0, c, block_nr,
				                          (unsigned char*)(buffer + offset), c, 0))
					throw Io_error();
			} else {
				if (reg_pio_data_in_lba48(dev_num(), CMD_READ_SECTORS_EXT, 0, c, 0, block_nr,
				                          (unsigned char*)(buffer + offset), c, 0))
					throw Io_error();
			}
		}

		count    -= c;
		block_nr += c;
		offset   += c * block_size();
	}
}


void Ata::Device::_write(Genode::size_t  block_nr,
                         Genode::size_t  count,
                         char const     *buffer,
                         bool            dma)
{
	Genode::size_t offset = 0;

	while (count > 0) {
		Genode::size_t c = count > 255 ? 255 : count;

		if (dma) {

			if (verbose)
				PDBG("DMA read: block %zu, c %zu, buffer: %p",
					 block_nr, c, (void*)(buffer + offset));

			if (!_lba48)
			{
				if (dma_pci_lba28(dev_num(), CMD_WRITE_DMA, 0, c, block_nr,
				                  (unsigned char*)(buffer + offset), c))
					throw Io_error();
			}
			else {
				if (dma_pci_lba48(dev_num(), CMD_WRITE_DMA_EXT, 0, c, 0, block_nr,
				                  (unsigned char*)(buffer + offset), c))
				throw Io_error();

			}
		}
		else {
			if (!_lba48)
			{
				if (reg_pio_data_out_lba28(dev_num(), CMD_WRITE_SECTORS, 0, c, block_nr,
				                           (unsigned char*)(buffer + offset), c, 0))
					throw Io_error();
			} else {
				if (reg_pio_data_out_lba48(dev_num(), CMD_WRITE_SECTORS_EXT, 0, c, 0, block_nr,
				                           (unsigned char*)(buffer + offset), c, 0))
					throw Io_error();
			}
		}

		count    -= c;
		block_nr += c;
		offset   += c * block_size();
	}
}


Ata::Device * Ata::Device::probe_legacy(int search_type)
{
	unsigned base[] = { 0x1f0, 0x170 };
	unsigned dev_irq[] = { 14, 15 };

	const char *type = "";
	for (int i = 0; i < 2; i++) {

		Device *dev = new (env()->heap() )Device(base[i], base[i] + 0x200);
		Device::current(dev);

		/* Scan for devices */
		reg_config();

		for (unsigned char j = 0; j < 2; j++) {

			switch (reg_config_info[j]) {

				case REG_CONFIG_TYPE_NONE:
					type = "none"; break;

				case REG_CONFIG_TYPE_UNKN:
					type = "unknown"; break;

				case REG_CONFIG_TYPE_ATA:
					type = "ATA"; break;

				case REG_CONFIG_TYPE_ATAPI:
					type = "ATAPI";
					destroy (env()->heap(), dev);
					dev = new (env()->heap() )Atapi_device(base[i], base[i] + 0x200);
					break;

				default:
					type = "";
				}

			Genode::printf("IDE %d Device %d: %s IRQ: %u\n", i, j, type, dev_irq[i]);

			/* prepare device */
			if (reg_config_info[j] == search_type) {

				dev->set_bus_master(i);
				dev->set_dev_num(j);
				dev->set_irq(dev_irq[i]);

				dev->probe_dma();
				Genode::printf("Device initialized! Enabling interrupts ...\n");
				int_use_intr_flag = 1;
				reg_reset(dev->dev_num());

				return dev;
			}
		}

		type = "";
		destroy(env()->heap(), dev);
	}

	return 0;
}


Block::Session::Operations Ata::Device::ops()
{
	Block::Session::Operations o;
	o.set_operation(Block::Packet_descriptor::READ);
	o.set_operation(Block::Packet_descriptor::WRITE);
	return o;
}
