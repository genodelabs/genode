/*
 * \brief  Test for PCI bus driver
 * \author Norman Feske
 * \date   2008-01-18
 */

/*
 * Copyright (C) 2008-2020 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <base/component.h>
#include <base/log.h>
#include <legacy/x86/platform_session/connection.h>
#include <legacy/x86/platform_device/client.h>
#include <util/retry.h>

using namespace Genode;

enum { AMD_VENDOR_ID = 0x1022, INTEL_VENDOR_ID = 0x8086 };
enum {
	CLASS_CODE_SATA           = 0x106,
	CLASS_CODE_ETHERNET       = 0x200,
	CLASS_CODE_VGA            = 0x300,
	CLASS_CODE_HOST_BRIDGE    = 0x600,
	CLASS_CODE_HOST_ISA       = 0x601,
	CLASS_CODE_PCI_PCI_BRIDGE = 0x604,
	CLASS_CODE_IOMMU          = 0x806,
	CLASS_CODE_USB            = 0xc03,
	CLASS_CODE_SMBUS          = 0xc05,
};

enum {
	USB_UHCI  = 0x00,
	USB_OHCI  = 0x10,
	USB_EHCI  = 0x20,
	USB_XHCI  = 0x30
};

enum {
	DEVICE_AMD_HUDSON2_SMBUS  = 0x780b,
};

enum {
	CAP_PWRM   = 0x01,
	CAP_MSI    = 0x05,
	CAP_HT     = 0x08,
	CAP_SECDEV = 0x0f,
	CAP_PCI_E  = 0x10,
	CAP_MSIX   = 0x11
};

enum {
	EXT_CAP_ERRREP = 0x01, /* advanced error reporting */
	EXT_CAP_DEVSNR = 0x03, /* device serial number */
	EXT_CAP_VENDOR = 0x0b, /* vendor specific */
	EXT_CAP_ACSERV = 0x0d, /* access control service */
};

typedef Platform::Device::Access_size Size;

static void dump_extended_pci_caps(Platform::Device_client &device)
{
	unsigned cap = 0x100;
	unsigned max_caps = (0x1000 - cap) / 8;

	String<128> cap_string { };
	while (max_caps) {
		unsigned const val = device.config_read(cap, Size::ACCESS_32BIT);
		switch (val & 0xffff) {
		case 0:
			break;
		case EXT_CAP_ERRREP:
			cap_string = String<128>(cap_string, " ERR_REP");
			break;
		case EXT_CAP_DEVSNR:
			cap_string = String<128>(cap_string, " DEV_SNR");
			break;
		case EXT_CAP_VENDOR:
			cap_string = String<128>(cap_string, " VENDOR");
			break;
		case EXT_CAP_ACSERV:
			cap_string = String<128>(cap_string, " ACS");
			break;
		default:
			cap_string = String<128>(cap_string, " ", Hex(val & 0xffff));
		};

		max_caps --;
		cap = (val >> 20) & 0xffc;
		if (cap <= 0x100 || cap >= 0x1000 - 4 /* size of 32Bit */)
			break;
	}

	if (cap_string.valid())
		log(" ECAP:", cap_string);
}

/**
 * Print device information
 */
static void print_device_info(Platform::Device_capability device_cap)
{
	if (!device_cap.valid()) {
		error("invalid device capability");
		return;
	}

	Platform::Device_client device(device_cap);

	unsigned char bus = 0, dev = 0, fun = 0;
	device.bus_address(&bus, &dev, &fun);
	unsigned short const vendor_id  = device.vendor_id();
	unsigned short const device_id  = device.device_id();
	unsigned       const class_code = device.class_code() >> 8;
	unsigned char  const revision   = device.config_read(0x8, Size::ACCESS_8BIT);
	unsigned char  const prog_if    = device.config_read(0x9, Size::ACCESS_8BIT);
	unsigned       const pci_cmd    = device.config_read(0x4, Size::ACCESS_16BIT);


	log(Hex(bus, Hex::OMIT_PREFIX), ":",
	    Hex(dev, Hex::OMIT_PREFIX), ".",
	    Hex(fun, Hex::OMIT_PREFIX), " "
	    "class=", Hex(class_code), " ",
	              (class_code == CLASS_CODE_VGA) ? "(VGA)" :
	              (class_code == CLASS_CODE_HOST_BRIDGE) ? "(bridge host)" :
	              (class_code == CLASS_CODE_HOST_ISA) ? "(bridge ISA)" :
	              (class_code == CLASS_CODE_PCI_PCI_BRIDGE) ? "(bridge PCI)" :
	              (class_code == CLASS_CODE_IOMMU) ? "(IOMMU)" :
	              (class_code == CLASS_CODE_USB) ? "(USB)" :
	              (class_code == CLASS_CODE_SMBUS) ? "(SMBUS)" :
	              (class_code == CLASS_CODE_SATA) ? "(SATA)" : "",
	              (class_code == CLASS_CODE_ETHERNET) ? "(Ethernet)" : "",
	    " vendor=", Hex(vendor_id), " ",
	               ((vendor_id == INTEL_VENDOR_ID) ? "(Intel)" :
	                (vendor_id == AMD_VENDOR_ID) ? "(AMD)" : "(unknown)"),
	    " device=", Hex(device_id),
	    " prog_if=", Genode::Hex(prog_if),
	                 (class_code != CLASS_CODE_USB) ? "" :
	                 (prog_if == USB_UHCI) ? "(UHCI)" :
	                 (prog_if == USB_OHCI) ? "(OHCI)" :
	                 (prog_if == USB_EHCI) ? "(EHCI)" :
	                 (prog_if == USB_XHCI) ? "(XHCI)" : "",
	    " revision=", Genode::Hex(revision),
	    " pci_cmd=", Genode::Hex(pci_cmd));

	if (vendor_id == AMD_VENDOR_ID && device_id == DEVICE_AMD_HUDSON2_SMBUS) {
		if (revision >= 0x11 && revision <= 0x14)
			log("chipset: AMD HUDSON2");
		else if (revision >= 0x15 && revision <= 0x18)
			log("chipset: AMD BOLTON");
		else if (revision >= 0x39 && revision <= 0x3a)
			log("chipset: AMD ANGTZE");
	}

	for (int resource_id = 0; resource_id < 6; resource_id++) {

		typedef Platform::Device::Resource Resource;

		Resource const resource = device.resource(resource_id);

		if (resource.type() != Resource::INVALID) {
			log("  Resource ", resource_id, " "
			    "(", (resource.type() == Resource::IO ? "I/O" : "MEM"), "): "
			    "base=", Genode::Hex(resource.base()), " "
			    "size=", Genode::Hex(resource.size()), " ",
			    (resource.prefetchable() ? "prefetchable" : "")
			);
		}
	}

	unsigned cap = device.config_read(0x34, Size::ACCESS_8BIT);
	if (cap) {
		String<128> cap_string { };
		for (Genode::uint32_t val = 0; cap; cap = (val >> 8) & 0xff) {
			val = device.config_read(cap, Size::ACCESS_32BIT);
			unsigned const type = val & 0xff;
			switch (type) {
			case CAP_MSI: {
				uint16_t msi_val = device.config_read(cap + 2, Size::ACCESS_16BIT);
				if (msi_val & 0x80)
					cap_string = String<128>(cap_string, " MSI-64");
				else
					cap_string = String<128>(cap_string, " MSI");
				break;
			}
			case CAP_HT:     cap_string = String<128>(cap_string, " HYPERTRANSPORT"); break;
			case CAP_MSIX:   cap_string = String<128>(cap_string, " MSI-X"); break;
			case CAP_PCI_E:  cap_string = String<128>(cap_string, " PCI-E"); break;
			case CAP_SECDEV: cap_string = String<128>(cap_string, " SECURE-DEVICE"); break;
			case CAP_PWRM:   cap_string = String<128>(cap_string, " PWRM"); break;
			default:
				cap_string = String<128>(cap_string, " ", Hex(type));
			}

			if (type == CAP_PCI_E) {
				uint16_t flags = device.config_read(cap + 2, Size::ACCESS_16BIT);
				uint8_t type   = (flags >> 4) & 0xf;
				cap_string = String<128>(cap_string, "(T", Hex(type, Hex::OMIT_PREFIX), ")");
			}
		}
		log("  CAP:", cap_string);
	}

	dump_extended_pci_caps(device);
}


void Component::construct(Genode::Env &env)
{
	log("--- Platform test started ---");

	/* open session to pci service */
	static Platform::Connection pci(env);

	Platform::Device_capability prev_device_cap, device_cap;

	pci.with_upgrade([&] () { device_cap = pci.first_device(); });

	/*
	 * Iterate through all installed devices
	 * and print the available device information.
	 */
	while (device_cap.valid()) {
		print_device_info(device_cap);

		pci.release_device(prev_device_cap);
		prev_device_cap = device_cap;

		pci.with_upgrade([&] () { device_cap = pci.next_device(device_cap); });
	}

	/* release last device */
	pci.release_device(prev_device_cap);

	log("--- Platform test finished ---");
}
