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

	enum { CMD_NATIVE_MAX_ADDRESS = 0xf8 };

	if (!reg_non_data_lba28(dev_num(), CMD_NATIVE_MAX_ADDRESS, 0, 1, 0UL)) {

		_block_end  =  _pio->inb(CB_SN);              /* 0 - 7 */
		_block_end |=  _pio->inb(CB_CL) << 8;         /* 8 - 15 */
		_block_end |=  _pio->inb(CB_CH) << 16;        /* 16 - 23 */
		_block_end |= (_pio->inb(CB_DH) & 0xf) << 24; /* 24 - 27 */
	}

	if (verbose)
		PDBG("First block: %u last block %u, block size %u",
		     _block_start, _block_end, _block_size);
}


void Ata::Device::read(unsigned long block_nr,
                       unsigned long count, off_t offset) {

	while (count > 0) {
		unsigned long c = count > 255 ? 255 : count;

		if (dma_enabled()) {

			if (verbose)
				PDBG("DMA read: block %lu, c %lu, offset: %08lx, base: %08lx",
					 block_nr, c, offset, _base_addr);

			if (dma_pci_lba28(dev_num(), CMD_READ_DMA, 0, c, block_nr,
							  (unsigned char *)(_base_addr + offset), c))
				throw Io_error();
		}
		else
			if (reg_pio_data_in_lba28(dev_num(), CMD_READ_SECTORS, 0, c, block_nr,
									  (unsigned char *)(_base_addr + offset), c, 0))
				throw Io_error();

		count    -= c;
		block_nr += c;
		offset   += c * block_size();
	}
}


void Ata::Device::write(unsigned long block_nr,
                        unsigned long count, off_t offset) {

	while (count > 0) {
		unsigned long c = count > 255 ? 255 : count;

		if (dma_enabled()) {

			if (verbose)
				PDBG("DMA read: block %lu, c %lu, offset: %08lx, base: %08lx",
					 block_nr, c, offset, _base_addr);

			if (dma_pci_lba28(dev_num(), CMD_WRITE_DMA, 0, c, block_nr,
							  (unsigned char *)(_base_addr + offset), c))
				throw Io_error();
		}
		else
			if (reg_pio_data_out_lba28(dev_num(), CMD_WRITE_SECTORS, 0, c, block_nr,
									   (unsigned char *)(_base_addr + offset), c, 0))
				throw Io_error();

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


