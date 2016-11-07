/*
 * \brief  DDE iPXE NIC API implementation
 * \author Christian Helmuth
 * \date   2010-09-13
 */

/*
 * Copyright (C) 2010-2015 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* iPXE */
#include <stdlib.h>
#include <ipxe/netdevice.h>
#include <ipxe/pci.h>
#include <ipxe/iobuf.h>

#include <dde_ipxe/nic.h>
/* local includes */
#include "local.h"
#include <dde_support.h>

/**
 * Network device driven by iPXE
 */
static struct net_device *net_dev;

/**
 * Callback function pointers
 */
static dde_ipxe_nic_link_cb link_callback;
static dde_ipxe_nic_rx_cb   rx_callback;

/**
 * Known iPXE driver structures (located in the driver binaries)
 */
extern struct pci_driver
	realtek_driver,
	ifec_driver,
	intel_driver,
	tg3_pci_driver;


/**
 * Driver database (used for probing)PCI_BASE_CLASS_NETWORK
 */
static struct pci_driver *pci_drivers[] = {
	&realtek_driver,
	&ifec_driver,
	&intel_driver,
	&tg3_pci_driver
};

/**
 * Update BARs of PCI device
 */
static void pci_read_bases(struct pci_device *pci_dev)
{
	uint32_t bar;
	int reg;
	uint8_t virt_bar_ioport = 0;

	for (reg = PCI_BASE_ADDRESS_0; reg <= PCI_BASE_ADDRESS_5; reg += 4) {
		pci_read_config_dword(pci_dev, reg, &bar);
		if (bar & PCI_BASE_ADDRESS_SPACE_IO) {
			if (!pci_dev->ioaddr) {
				pci_dev->ioaddr = bar & PCI_BASE_ADDRESS_IO_MASK;

				dde_request_io(virt_bar_ioport);
			}
			virt_bar_ioport ++;
		} else {
			if (!pci_dev->membase)
				pci_dev->membase = bar & PCI_BASE_ADDRESS_MEM_MASK;
			/* Skip next BAR if 64-bit */
			if (bar & PCI_BASE_ADDRESS_MEM_TYPE_64)
				reg += 4;
		}
	}
}


/**
 * Probe one PCI device
 */
static int probe_pci_device(struct pci_device *pci_dev)
{
	int j;
	for (j = 0; j < sizeof(pci_drivers)/sizeof(*pci_drivers); j++) {
		struct pci_driver *driver = pci_drivers[j];

		int i;
		for (i = 0; i < driver->id_count; i++) {
			struct pci_device_id *id = &driver->ids[i];
			if ((id->vendor != PCI_ANY_ID) && (id->vendor != pci_dev->vendor))
				continue;
			if ((id->device != PCI_ANY_ID) && (id->device != pci_dev->device))
				continue;
			pci_set_driver(pci_dev, driver, id);

			LOG("using driver %s", pci_dev->id->name);
			int ret = driver->probe(pci_dev);
			if (ret != 0) {
				LOG("probe failed for %s", pci_dev->id->name);
				continue;
			}
			return 0;
		}
	}

	LOG("no driver found");
	return -1;
}


enum { NO_DEVICE_FOUND = ~0U };

/**
 * Scan the PCI bus
 *
 * \return PCI location of NIC found; NO_DEVICE_FOUND otherwise
 */
static unsigned scan_pci(void)
{
	int ret, bus = 0, dev = 0, fun = 0; 
	for (ret = dde_pci_first_device(&bus, &dev, &fun);
	     ret == 0;
	     ret = dde_pci_next_device(&bus, &dev, &fun)) {

		dde_uint32_t class_code;
		dde_pci_readl(PCI_CLASS_REVISION, &class_code);
		class_code >>= 8;
		if (PCI_BASE_CLASS(class_code) != PCI_BASE_CLASS_NETWORK)
			continue;

		dde_uint16_t vendor, device;
		dde_pci_readw(PCI_VENDOR_ID, &vendor);
		dde_pci_readw(PCI_DEVICE_ID, &device);
		dde_uint8_t rev, irq;
		dde_pci_readb(PCI_REVISION_ID, &rev);
		dde_pci_readb(PCI_INTERRUPT_LINE, &irq);
		LOG("Found: " FMT_BUSDEVFN " %04x:%04x (rev %02x) IRQ %02x",
		            bus, dev, fun, vendor, device, rev, irq);

		struct pci_device *pci_dev = zalloc(sizeof(*pci_dev));

		pci_dev->busdevfn = PCI_BUSDEVFN(bus, dev, fun);
		pci_dev->vendor   = vendor;
		pci_dev->device   = device;
		pci_dev->class    = class_code;
		pci_dev->irq      = irq;

		pci_read_bases(pci_dev);

		pci_dev->dev.desc.bus_type = BUS_TYPE_PCI;
		pci_dev->dev.desc.location = pci_dev->busdevfn;
		pci_dev->dev.desc.vendor   = pci_dev->vendor;
		pci_dev->dev.desc.device   = pci_dev->device;
		pci_dev->dev.desc.class    = pci_dev->class;
		pci_dev->dev.desc.ioaddr   = pci_dev->ioaddr;
		pci_dev->dev.desc.irq      = pci_dev->irq;

		/* we found our device -> break loop */
		if (!probe_pci_device(pci_dev))
			return pci_dev->dev.desc.location;

		/* free device if no driver was found */
		free(pci_dev);
	}

	return NO_DEVICE_FOUND;
}


/**
 * IRQ handler registered at DDE
 */
static void irq_handler(void *p)
{
	dde_lock_enter();

	/* check for the link-state to change on each interrupt */
	int link_ok = netdev_link_ok(net_dev);

	/* poll the device for packets and also link-state changes */
	netdev_poll(net_dev);

	struct io_buffer *iobuf;
	while ((iobuf = netdev_rx_dequeue(net_dev))) {
		dde_lock_leave();
		if (rx_callback)
			rx_callback(1, iobuf->data, iob_len(iobuf));
		dde_lock_enter();
		free_iob(iobuf);
	}

	dde_lock_leave();

	if (link_ok != netdev_link_ok(net_dev))
		/* report link-state changes */
		if (link_callback)
			link_callback();
}


/************************
 ** API implementation **
 ************************/

void dde_ipxe_nic_register_callbacks(dde_ipxe_nic_rx_cb rx_cb,
                                     dde_ipxe_nic_link_cb link_cb)
{
	dde_lock_enter();

	rx_callback   = rx_cb;
	link_callback = link_cb;

	dde_lock_leave();
}


void dde_ipxe_nic_unregister_callbacks(void)
{
	dde_lock_enter();

	rx_callback   = (dde_ipxe_nic_rx_cb)0;
	link_callback = (dde_ipxe_nic_link_cb)0;

	dde_lock_leave();
}


int dde_ipxe_nic_link_state(unsigned if_index)
{
	if (if_index != 1)
		return -1;

	dde_lock_enter();

	int link_state = netdev_link_ok(net_dev);

	dde_lock_leave();
	return link_state;
}


int dde_ipxe_nic_tx(unsigned if_index, const char *packet, unsigned packet_len)
{
	if (if_index != 1)
		return -1;

	dde_lock_enter();

	struct io_buffer *iobuf = alloc_iob(packet_len);

	dde_lock_leave();

	if (!iobuf)
		return -1;

	memcpy(iob_put(iobuf, packet_len), packet, packet_len);
	dde_lock_enter();

	netdev_poll(net_dev);
	netdev_tx(net_dev, iob_disown(iobuf));

	dde_lock_leave();
	return 0;
}


int dde_ipxe_nic_get_mac_addr(unsigned if_index, unsigned char *out_mac_addr)
{
	if (if_index != 1)
		return -1;

	dde_lock_enter();

	out_mac_addr[0] = net_dev->hw_addr[0];
	out_mac_addr[1] = net_dev->hw_addr[1];
	out_mac_addr[2] = net_dev->hw_addr[2];
	out_mac_addr[3] = net_dev->hw_addr[3];
	out_mac_addr[4] = net_dev->hw_addr[4];
	out_mac_addr[5] = net_dev->hw_addr[5];

	dde_lock_leave();
	return 0;
}


int dde_ipxe_nic_init(void *ep)
{
	dde_init(ep);

	dde_lock_enter();

	/* scan all pci devices and drivers */
	unsigned location = scan_pci();
	if (location == NO_DEVICE_FOUND)
		return 0;

	/* find iPXE NIC device */
	net_dev = find_netdev_by_location(BUS_TYPE_PCI, location);

	/* open iPXE NIC device */
	if (netdev_open(net_dev)) {
		LOG("opening device " FMT_BUSDEVFN " failed",
		    PCI_BUS(net_dev->dev->desc.location),
		    PCI_SLOT(net_dev->dev->desc.location),
		    PCI_FUNC(net_dev->dev->desc.location));
		return 0;
	}

	/* initialize IRQ handler */
	int err = dde_interrupt_attach(irq_handler, 0);
	if (err) {
		LOG("attaching to IRQ %02x failed", net_dev->dev->desc.irq);
		return 0;
	}
	netdev_irq(net_dev, 1);

	dde_lock_leave();

	/* always report 1 device was found */
	return 1;
}
