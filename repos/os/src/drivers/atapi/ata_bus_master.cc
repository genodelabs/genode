/*
 * \brief  I/O interface to the IDE bus master
 * \author Sebastian Sumpf <Sebastian.Sumpf@genode-labs.com>
 * \date   2010-07-15
 */

/*
 * Copyright (C) 2010-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <base/printf.h>
#include <io_port_session/connection.h>
#include <pci_session/connection.h>
#include <pci_device/client.h>

/* local includes */
#include "ata_bus_master.h"
#include "mindrvr.h"

using namespace Genode;


Ata::Bus_master::Bus_master(bool secondary) : _bmiba(0), _secondary(secondary)
{}


Ata::Bus_master::~Bus_master()
{
	destroy(env()->heap(), _pio);
}


bool Ata::Bus_master::scan_pci()
{
	Pci::Connection pci;
	Pci::Device_capability prev_device_cap,
	                            device_cap = pci.first_device();

	/* test if standard legacy ports are used, if not skip */
	uint32_t legacy = _secondary ? PI_CH2_LEGACY : PI_CH1_LEGACY;

	while (device_cap.valid()) {
		pci.release_device(prev_device_cap);
		prev_device_cap = device_cap;

		::Pci::Device_client device(device_cap);

		if ((device.class_code() & CLASS_MASK)
		    ==  (CLASS_MASS_STORAGE | SUBCLASS_IDE) &&
		    !(device.class_code() & legacy)) {
			Genode::printf("Found IDE Bus Master (Vendor ID: %04x Device ID: %04x Class: %08x)\n",
			               device.vendor_id(), device.device_id(), device.class_code());

			/* read BMIBA */
			_bmiba = device.config_read(PCI_CFG_BMIBA_OFF,
			                            ::Pci::Device::ACCESS_32BIT);

			if (_bmiba == 0xffff)
				_bmiba = 0;

			/* port I/O or memory mapped ? */
			_port_io = _bmiba & 0x1 ? true : false;

			/* XXX cbass: This may only be true for Intel IDE controllers */
			if (_bmiba && _port_io) {
				_bmiba &= 0xfff0;
				_pio = new(env()->heap()) Genode::Io_port_connection(_bmiba, 0x10);

				if (_secondary)
					_bmiba += 0x8;
			}

			Genode::printf("\tBus master interface base addr: %08x (%s) secondary (%s) (%s)\n",
			               _bmiba, _port_io ? "I/O" : "MEM",
			               _secondary ? "yes" : "no",
			               _bmiba ? "supported" : "invalid");

			pci.release_device(prev_device_cap);
			return _bmiba;
		}

		device_cap = pci.next_device(prev_device_cap);
	}

	/* release last device */
	pci.release_device(prev_device_cap);

	return _bmiba;
}


unsigned char Ata::Bus_master::read_cmd()
{
	if (!_bmiba || !_port_io)
		return 0;

	return _pio->inb(_bmiba + BM_COMMAND_REG);
}


unsigned char Ata::Bus_master::read_status()
{
	if (!_bmiba || !_port_io)
		return 0;

	return _pio->inb(_bmiba + BM_STATUS_REG);
}


void Ata::Bus_master::write_cmd(unsigned char val)
{
	if (!_bmiba || !_port_io)
		return;

	_pio->outb(_bmiba + BM_COMMAND_REG, val);
}


void Ata::Bus_master::write_status(unsigned char val)
{
	if (!_bmiba || !_port_io)
		return;

	_pio->outb(_bmiba + BM_STATUS_REG, val);
}


void Ata::Bus_master::write_prd(unsigned long val)
{
	if (!_bmiba || !_port_io)
		return;

	if (val == _prd_virt) {
		_pio->outl(_bmiba + BM_PRD_ADDR_LOW, _prd_phys);
	}
}
