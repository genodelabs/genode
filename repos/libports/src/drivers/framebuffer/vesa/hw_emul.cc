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
 * Copyright (C) 2010-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <base/log.h>
#include <util/retry.h>

/* local includes */
#include "framebuffer.h"
#include "hw_emul.h"

using namespace Genode;

static const bool verbose = false;

/************************
 ** PCI virtualization **
 ************************/

#include <platform_session/device.h>

enum {
	PCI_ADDR_REG = 0xcf8,
	PCI_DATA_REG = 0xcfc
};


class Pci_card
{
	private:

		enum { BAR_MAX = 6 };

		Platform::Connection _pci_drv;
		Platform::Device     _device           { _pci_drv };
		unsigned             _vendor_device_id { 0 };
		unsigned             _class_code       { 0 };
		uint32_t             _bar[BAR_MAX]     { 0xffffffff};

	public:

		struct Invalid_bar : Exception {};

		Pci_card(Genode::Env &env) : _pci_drv(env)
		{
			_pci_drv.update();
			_pci_drv.with_xml([&] (Xml_node node) {
				node.with_optional_sub_node("device", [&] (Xml_node node) {
					node.for_each_sub_node("io_mem", [&] (Xml_node node) {
						unsigned bar  = node.attribute_value("pci_bar", 0U);
						uint32_t addr = node.attribute_value("phys_addr", 0UL);
						if (bar >= BAR_MAX) throw Invalid_bar();
						_bar[bar] = addr;
					});
					node.for_each_sub_node("io_port_range", [&] (Xml_node node) {
						unsigned bar = node.attribute_value("pci_bar", 0U);
						uint32_t addr = node.attribute_value("phys_addr", 0UL);
						if (bar >= BAR_MAX) throw Invalid_bar();
						_bar[bar] = addr | 1;
					});
					node.with_optional_sub_node("pci-config", [&] (Xml_node node) {
						unsigned v = node.attribute_value("vendor_id", 0U);
						unsigned d = node.attribute_value("device_id", 0U);
						unsigned c = node.attribute_value("class",     0U);
						unsigned r = node.attribute_value("revision",  0U);
						_vendor_device_id = v | d << 16;
						_class_code       = r | c << 8;
					});
				});
			});
		}

		unsigned vendor_device_id() { return _vendor_device_id; }
		unsigned class_code()       { return _class_code;       }

		uint32_t bar(unsigned bar)
		{
			if (bar >= BAR_MAX) throw Invalid_bar();
			return _bar[bar];
		}
};


static Constructible<Pci_card> pci_card;


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
				raw_val = pci_card->vendor_device_id();
				break;

			case 4: /* status and command */
			case 8: /* class code / revision ID */
				raw_val = pci_card->class_code();
				break;

			case 0x10: /* base address register 0 */
			case 0x14: /* base address register 1 */
			case 0x18: /* base address register 2 */
			case 0x1c: /* base address register 3 */
			case 0x20: /* base address register 4 */
			case 0x24: /* base address register 5 */
				{
				raw_val = pci_card->bar((pci_cfg_addr-0x10)/4);
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


void hw_emul_init(Genode::Env &env) { pci_card.construct(env); }
