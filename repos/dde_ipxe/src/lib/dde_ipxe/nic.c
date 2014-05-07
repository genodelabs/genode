/*
 * \brief  DDE iPXE NIC API implementation
 * \author Christian Helmuth
 * \date   2010-09-13
 */

/*
 * Copyright (C) 2010-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* DDE kit */
#include <dde_kit/pci.h>
#include <dde_kit/lock.h>
#include <dde_kit/semaphore.h>
#include <dde_kit/timer.h>
#include <dde_kit/interrupt.h>
#include <dde_kit/dde_kit.h>

/* iPXE */
#include <stdlib.h>
#include <ipxe/netdevice.h>
#include <ipxe/pci.h>
#include <ipxe/iobuf.h>

#include <dde_ipxe/nic.h>
#include "local.h"
#include "dde_support.h"

/**
 * DDE iPXE mutual exclusion lock
 */
static struct dde_kit_lock *ipxe_lock;

#define ENTER dde_kit_lock_lock(ipxe_lock)
#define LEAVE dde_kit_lock_unlock(ipxe_lock)

/**
 * Bottom-half activation semaphore
 */
static struct dde_kit_sem *bh_sema;

/**
 * Network device driven by iPXE
 */
static struct net_device *net_dev;

/**
 * RX callback function pointer
 */
static dde_ipxe_nic_rx_cb rx_callback;

/**
 * Known iPXE driver structures (located in the driver binaries)
 */
extern struct pci_driver
	realtek_driver,
	ifec_driver,
	intel_driver,
	pcnet32_driver;

/**
 * Driver database (used for probing)
 */
static struct pci_driver *pci_drivers[] = {
	&realtek_driver,
	&ifec_driver,
	&intel_driver,
	&pcnet32_driver
};


/**
 * Update BARs of PCI device
 */
static void pci_read_bases(struct pci_device *pci_dev)
{
	uint32_t bar;
	int reg;

	for (reg = PCI_BASE_ADDRESS_0; reg <= PCI_BASE_ADDRESS_5; reg += 4) {
		pci_read_config_dword(pci_dev, reg, &bar);
		if (bar & PCI_BASE_ADDRESS_SPACE_IO) {
			if (!pci_dev->ioaddr) {
				pci_dev->ioaddr = bar & PCI_BASE_ADDRESS_IO_MASK;

				dde_kit_addr_t base = bar & PCI_BASE_ADDRESS_IO_MASK;
				dde_kit_size_t size = pci_bar_size(pci_dev, reg);
				dde_kit_request_io(base, size);
			}
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
	for (ret = dde_kit_pci_first_device(&bus, &dev, &fun);
	     ret == 0;
	     ret = dde_kit_pci_next_device(&bus, &dev, &fun)) {

		dde_kit_uint32_t class_code;
		dde_kit_pci_readl(bus, dev, fun, PCI_CLASS_REVISION, &class_code);
		class_code >>= 8;
		if (PCI_BASE_CLASS(class_code) != PCI_BASE_CLASS_NETWORK)
			continue;

		dde_kit_uint16_t vendor, device;
		dde_kit_pci_readw(bus, dev, fun, PCI_VENDOR_ID, &vendor);
		dde_kit_pci_readw(bus, dev, fun, PCI_DEVICE_ID, &device);
		dde_kit_uint8_t rev, irq;
		dde_kit_pci_readb(bus, dev, fun, PCI_REVISION_ID, &rev);
		dde_kit_pci_readb(bus, dev, fun, PCI_INTERRUPT_LINE, &irq);
		LOG("Found: " FMT_BUSDEVFN " %04x:%04x (rev %02x) IRQ %02x",
		            bus, dev, fun, vendor, device, rev, irq);

		struct pci_device *pci_dev = zalloc(sizeof(*pci_dev));
		ASSERT(pci_dev != 0);

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
 * IRQ handler registered at DDE kit
 */
static void irq_handler(void *p)
{
	ENTER;

	netdev_poll(net_dev);
	dde_kit_sem_up(bh_sema);

	LEAVE;
}


/**
 * Bottom-half handler executed in separate thread
 *
 * Calls RX callback if appropriate.
 */
static void bh_handler(void *p)
{
	while (1) {
		dde_kit_sem_down(bh_sema);

		ENTER;

		struct io_buffer *iobuf;
		while ((iobuf = netdev_rx_dequeue(net_dev))) {
			LEAVE;
			if (rx_callback)
				rx_callback(1, iobuf->data, iob_len(iobuf));
			ENTER;
			free_iob(iobuf);
		}

		LEAVE;
	}
}


/************************
 ** API implementation **
 ************************/

dde_ipxe_nic_rx_cb dde_ipxe_nic_register_rx_callback(dde_ipxe_nic_rx_cb cb)
{
	ENTER;

	dde_ipxe_nic_rx_cb old = rx_callback;
	rx_callback            = cb;

	LEAVE;
	return old;
}


int dde_ipxe_nic_tx(unsigned if_index, const char *packet, unsigned packet_len)
{
	if (if_index != 1)
		return -1;

	ENTER;

	struct io_buffer *iobuf = alloc_iob(packet_len);

	LEAVE;

	if (!iobuf)
		return -1;

	memcpy(iob_put(iobuf, packet_len), packet, packet_len);
	ENTER;

	netdev_poll(net_dev);
	netdev_tx(net_dev, iob_disown(iobuf));

	LEAVE;
	return 0;
}


int dde_ipxe_nic_get_mac_addr(unsigned if_index, char *out_mac_addr)
{
	if (if_index != 1)
		return -1;

	ENTER;

	out_mac_addr[0] = net_dev->hw_addr[0];
	out_mac_addr[1] = net_dev->hw_addr[1];
	out_mac_addr[2] = net_dev->hw_addr[2];
	out_mac_addr[3] = net_dev->hw_addr[3];
	out_mac_addr[4] = net_dev->hw_addr[4];
	out_mac_addr[5] = net_dev->hw_addr[5];

	LEAVE;
	return 0;
}


int dde_ipxe_nic_init(void)
{
	dde_kit_init();
	dde_kit_timer_init(0, 0);
	enum {
		CLASS_MASK  = 0xff0000,
		CLASS_NETWORK = PCI_BASE_CLASS_NETWORK << 16
	};
	dde_kit_pci_init(CLASS_NETWORK, CLASS_MASK);

	dde_kit_lock_init(&ipxe_lock);

	slab_init();

	ENTER;

	/* scan all pci devices and drivers */
	unsigned location = scan_pci();
	if (location == NO_DEVICE_FOUND)
		return 0;

	/* find iPXE NIC device */
	net_dev = find_netdev_by_location(BUS_TYPE_PCI, location);

	/* initialize memory backend allocator for nic driver */
	if (!dde_mem_init(PCI_BUS(net_dev->dev->desc.location),
	                  PCI_SLOT(net_dev->dev->desc.location),
	                  PCI_FUNC(net_dev->dev->desc.location))) {
		LOG("initialization of block memory failed!");
		return 0;
	}

	/* open iPXE NIC device */
	if (netdev_open(net_dev)) {
		LOG("opening device " FMT_BUSDEVFN " failed",
		    PCI_BUS(net_dev->dev->desc.location),
		    PCI_SLOT(net_dev->dev->desc.location),
		    PCI_FUNC(net_dev->dev->desc.location));
		return 0;
	}

	/* initialize IRQ handler and enable interrupt/bottom-half handling */
	bh_sema = dde_kit_sem_init(0);
	dde_kit_thread_create(bh_handler, 0, "bh_handler");
	int err = dde_kit_interrupt_attach(net_dev->dev->desc.irq, 0,
	                                   0, irq_handler, 0);
	if (err) {
		LOG("attaching to IRQ %02x failed", net_dev->dev->desc.irq);
		return 0;
	}
	netdev_irq(net_dev, 1);

	LEAVE;

	/* always report 1 device was found */
	return 1;
}
