/*
 * \brief  Hardware emulation implementation
 * \author Christian Helmuth
 * \author Stefan Kalkowski
 * \date   2010-06-01
 *
 * The emulation comprises:
 *
 * - Simple programmable interval timer (PIT)
 * - Virtual PCI bus with VGA card attached to its physical slot
 */

/*
 * Copyright (C) 2010-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#include <base/sleep.h>
#include <base/log.h>

#include "hw_emul.h"

using namespace Genode;

static const bool verbose = false;

/************************
 ** PCI virtualization **
 ************************/

#include <platform_session/connection.h>
#include <platform_device/client.h>

enum {
	PCI_ADDR_REG = 0xcf8,
	PCI_DATA_REG = 0xcfc
};


/**
 * Helper for the formatted output of a (bus, device, function) triple
 */
struct Devfn
{
	unsigned short devfn;

	explicit Devfn(unsigned short devfn) : devfn(devfn) { }

	void print(Genode::Output &out) const
	{
		unsigned char const bus =  devfn >> 8,
		                    dev = (devfn >> 3) & 0x1f,
		                    fn  =  devfn & 0x7;

		Genode::print(out, Hex(bus, Hex::OMIT_PREFIX, Hex::PAD), ":",
		                   Hex(dev, Hex::OMIT_PREFIX, Hex::PAD), ".",
		                   Hex(fn,  Hex::OMIT_PREFIX));
	}
};


class Pci_card
{
	private:

		Platform::Connection    _pci_drv;
		Platform::Device_client _device;
		unsigned short     _devfn;

		Platform::Device_capability _find_vga_card()
		{
			/*
			 * Iterate through all accessible devices.
			 */
			Platform::Device_capability prev_device_cap, device_cap;
			Genode::env()->parent()->upgrade(_pci_drv.cap(), "ram_quota=4096");
			for (device_cap = _pci_drv.first_device();
			     device_cap.valid();
			     device_cap = _pci_drv.next_device(prev_device_cap)) {

				Platform::Device_client device(device_cap);

				if (prev_device_cap.valid())
					_pci_drv.release_device(prev_device_cap);
				/*
				 * If the device is an VGA compatible controller with base
				 * class 0x03 and sub class 0x00 stop iteration. (We shift out
				 * the interface bits.)
				 */
				if ((device.class_code() >> 8) == 0x0300)
					break;

				prev_device_cap = device_cap;
			}

			if (!device_cap.valid()) {
				Genode::error("PCI VGA card not found. Sleeping...");
				sleep_forever();
			}

			return device_cap;
		}

	public:

		Pci_card() : _pci_drv(), _device(_find_vga_card())
		{
			unsigned char bus = 0, dev = 0, fn = 0;

			_device.bus_address(&bus, &dev, &fn);
			_devfn = bus << 8 | dev << 3 | fn;

			if (verbose)
				Genode::log("Found PCI VGA at ", Devfn(_devfn));
		}

		Platform::Device_client &device() { return _device; }
		unsigned short devfn()  const { return _devfn; }
};


static Pci_card *pci_card()
{
	static Pci_card _pci_card;
	return &_pci_card;
}


/**
 * Last address written to address port.
 */
static unsigned pci_cfg_addr;
static bool     pci_cfg_addr_valid = false;


template <typename T>
static bool handle_pci_port_write(unsigned short port, T val)
{
	switch (port) {

	case PCI_ADDR_REG:
		/*
		 * The virtual bus has only 1 device - the VGA card - at its physical
		 * bus address.
		 */
		{
			if (sizeof(T) != 4) {
				warning("writing with size ", sizeof(T), " not supported", sizeof(T));
				return true;
			}

			unsigned const devfn = (val >> 8) & 0xffff;
			if (devfn != pci_card()->devfn()) {
				if (verbose)
					warning("accessing unknown PCI device ", Devfn(devfn));
				pci_cfg_addr_valid = false;
				return true;
			}

			/* remember the configuration address */
			pci_cfg_addr       = val & 0xfc;
			pci_cfg_addr_valid = true;

			return true;
		}

	case PCI_DATA_REG:
		warning("writing data register not supported (value=", Hex(val), ")");
		return true;

	default:
		return false;
	}
}


template <typename T>
static bool handle_pci_port_read(unsigned short port, T *val)
{
	unsigned raw_val;

	*val = 0;

	switch (port & ~3) {

	case PCI_ADDR_REG:
		*val = pci_cfg_addr;
		return true;

	case PCI_DATA_REG:
		{
			unsigned off_bits = (port & 3) * 8;

			if (!pci_cfg_addr_valid) {
				/*
				 * XXX returning -1 here for "invalid PCI device" breaks Qemu's
				 * Cirrus VGA BIOS.
				 */
				*val = 0;
				return true;
			}

			switch (pci_cfg_addr) {

			case 0: /* vendor / device ID */
				raw_val = pci_card()->device().vendor_id() |
				         (pci_card()->device().device_id() << 16);
				break;

			case 4: /* status and command */
			case 8: /* class code / revision ID */
				raw_val = pci_card()->device().config_read(pci_cfg_addr,
				                                           Platform::Device::ACCESS_32BIT);
				break;

			case 0x10: /* base address register 0 */
			case 0x14: /* base address register 1 */
			case 0x18: /* base address register 2 */
			case 0x1c: /* base address register 3 */
			case 0x20: /* base address register 4 */
			case 0x24: /* base address register 5 */
				{
					unsigned bar = (pci_cfg_addr - 0x10) / 4;
					Platform::Device::Resource res = pci_card()->device().resource(bar);
					if (res.type() == Platform::Device::Resource::INVALID) {
						warning("requested PCI resource ", bar, " invalid");
						*val = 0;
						return true;
					}

					raw_val = res.bar();
					break;
				}

			default:
				warning("unexpected configuration address ", Hex(pci_cfg_addr));
				return true;
			}

			*val = raw_val >> off_bits;
			return true;
		}

	default:
		return false;
	}
}


/************************
 ** PIT virtualization **
 ************************/

/*
 * Some VESA BIOS implementations use the PIT as time source. However, usually
 * only the PIT counter is queried by first writing a latch command (0) to the
 * command register and subsequently reading the data port two times (low word
 * and high word). Returning non-zero bogus values seems to make (at least
 * some) VESA BIOS implementations happy.
 */

enum {
	PIT_DATA_PORT_0 = 0x40,
	PIT_CMD_PORT    = 0x43,
};

/**
 * Handle port-write access to the PIT
 *
 * \return true if the port access referred to the PIT
 */
template <typename T>
static bool handle_pit_port_write(unsigned short port, T val)
{
	if (port >= PIT_DATA_PORT_0 && port <= PIT_CMD_PORT)
		return true;

	return false;
}


/**
 * Handle port-read access from the PIT registers
 *
 * \return true if the port access referred to the PIT
 */
template <typename T>
static bool handle_pit_port_read(unsigned short port, T *val)
{
	if (port >= PIT_DATA_PORT_0 && port <= PIT_CMD_PORT) {
		*val = 0x15; /* bogus value */
		return true;
	}

	return false;
}


/************************
 ** API implementation **
 ************************/

/* instantiate externally used template funciton */
template bool hw_emul_handle_port_read(unsigned short port, unsigned char *val);
template bool hw_emul_handle_port_read(unsigned short port, unsigned short *val);
template bool hw_emul_handle_port_read(unsigned short port, unsigned long *val);
template bool hw_emul_handle_port_read(unsigned short port, unsigned int *val);
template bool hw_emul_handle_port_write(unsigned short port, unsigned char val);
template bool hw_emul_handle_port_write(unsigned short port, unsigned short val);
template bool hw_emul_handle_port_write(unsigned short port, unsigned long val);
template bool hw_emul_handle_port_write(unsigned short port, unsigned int val);

template <typename T>
bool hw_emul_handle_port_read(unsigned short port, T *val)
{
	T ret;

	if (handle_pci_port_read(port, &ret)
	 || handle_pit_port_read(port, &ret)) {
		*val = ret;
		return true;
	}

	return false;
}

template <typename T>
bool hw_emul_handle_port_write(unsigned short port, T val)
{
	if (handle_pci_port_write(port, val)
	 || handle_pit_port_write(port, val))
		return true;

	return false;
}
