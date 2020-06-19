/*
 * \brief  Audio driver BSD API emulation
 * \author Josef Soentgen
 * \date   2014-11-16
 */

/*
 * Copyright (C) 2014-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <base/allocator_avl.h>
#include <base/log.h>
#include <base/object_pool.h>
#include <dataspace/client.h>
#include <io_port_session/connection.h>
#include <io_mem_session/connection.h>
#include <platform_session/connection.h>
#include <platform_device/client.h>
#include <util/retry.h>

/* local includes */
#include <bsd.h>
#include <bsd_emul.h>

#include <extern_c_begin.h>
# include <dev/pci/pcidevs.h>
#include <extern_c_end.h>


extern "C" int probe_cfdata(struct pci_attach_args *);

namespace {

class Pci_driver : public Bsd::Bus_driver
{
	public:

		enum Pci_config { IRQ = 0x3c, CMD = 0x4,
		                  CMD_IO = 0x1, CMD_MEMORY = 0x2, CMD_MASTER = 0x4 };

	private:

		Genode::Env       &_env;
		Genode::Allocator &_alloc;

		struct pci_attach_args _pa { 0, 0, 0, 0, 0 };

		Platform::Connection        _pci { _env };
		Platform::Device_capability _cap;

		Genode::Io_port_connection *_io_port { nullptr };

		/**
		 * The Dma_region_manager provides memory used for DMA
		 * and manages its mappings.
		 */
		struct Dma_region_manager : public Genode::Allocator_avl
		{
			Genode::Env &env;

			enum { BACKING_STORE_SIZE = 1024 * 1024 };

			Genode::addr_t base;
			Genode::addr_t mapped_base;

			bool _dma_initialized { false };

			Pci_driver &_drv;

			Dma_region_manager(Genode::Env       &env,
			                   Genode::Allocator &alloc,
			                   Pci_driver &drv)
			: Genode::Allocator_avl(&alloc), env(env), _drv(drv) { }

			Genode::addr_t alloc(Genode::size_t size, int align)
			{
				using namespace Genode;

				if (!_dma_initialized) {
					try {
						Ram_dataspace_capability cap = _drv._alloc_dma_memory(BACKING_STORE_SIZE);
						mapped_base = (addr_t)env.rm().attach(cap);
						base        = Dataspace_client(cap).phys_addr();

						Allocator_avl::add_range(mapped_base, BACKING_STORE_SIZE);
					} catch (...) {
						Genode::error("alloc DMA memory failed");
						return 0;
					}
					_dma_initialized = true;
				}

				void *ptr = nullptr;
				bool  err = Allocator_avl::alloc_aligned(size, &ptr, align).error();

				return err ? 0 : (addr_t)ptr;
			}

			void free(Genode::addr_t virt, Genode::size_t size) {
				Genode::Allocator_avl::free((void*)virt, size); }

			Genode::addr_t virt_to_phys(Genode::addr_t virt) {
				return virt - mapped_base + base; }

			Genode::addr_t phys_to_virt(Genode::addr_t phys) {
				return phys - base + mapped_base; }

		} _dma_region_manager;

		/**
		 * Scan pci bus for sound devices
		 */
		Platform::Device_capability _scan_pci(Platform::Device_capability const &prev)
		{
			Platform::Device_capability cap;
			/* shift values for Pci interface used by Genode */
			cap = _pci.with_upgrade([&] () {
				return _pci.next_device(prev,
				                        PCI_CLASS_MULTIMEDIA << 16,
				                        PCI_CLASS_MASK << 16); });
			
			if (prev.valid())
				_pci.release_device(prev);
			return cap;
		}

		/**
		 * Allocate DMA memory from the PCI driver
		 */
		Genode::Ram_dataspace_capability _alloc_dma_memory(Genode::size_t size)
		{
			size_t donate = size;

			return Genode::retry<Genode::Out_of_ram>(
				[&] () {
					return Genode::retry<Genode::Out_of_caps>(
						[&] () { return _pci.alloc_dma_buffer(size); },
						[&] () { _pci.upgrade_caps(2); });
				},
				[&] () {
					_pci.upgrade_ram(donate);
					donate = donate * 2 > size ? 4096 : donate * 2;
				});
		}

	public:

		Pci_driver(Genode::Env &env, Genode::Allocator &alloc)
		:
			_env(env), _alloc(alloc),
			_dma_region_manager(_env, _alloc, *this)
		{ }

		Genode::Env &env() { return _env; }

		Genode::Allocator &alloc() { return _alloc; }

		Platform::Device_capability cap() { return _cap; }

		Platform::Connection &pci() { return _pci; }

		int probe()
		{
			_pci.upgrade_ram(8*1024);

			/*
			 * We hide ourself in the bus_dma_tag_t as well as
			 * in the pci_chipset_tag_t field because they are
			 * used in all pci or bus related functions and are
			 * our access window, hence.
			 */
			_pa.pa_dmat = (bus_dma_tag_t)this;
			_pa.pa_pc   = (pci_chipset_tag_t)this;

			int found = 0;
			while ((_cap = _scan_pci(_cap)).valid()) {
				Platform::Device_client device(_cap);

				uint8_t bus, dev, func;
				device.bus_address(&bus, &dev, &func);

				if ((device.device_id() == PCI_PRODUCT_INTEL_CORE4G_HDA_2) ||
				    (device.vendor_id() == PCI_VENDOR_INTEL &&
				     bus == 0 && dev == 3 && func == 0)) {
					Genode::warning("ignore ", (unsigned)bus, ":", (unsigned)dev, ":",
					                (unsigned)func, ", not supported HDMI/DP HDA device");
					continue;
				}

				/* we do the shifting to match OpenBSD's assumptions */
				_pa.pa_tag   = 0x80000000UL | (bus << 16) | (dev << 11) | (func << 8);
				_pa.pa_class = device.class_code() << 8;
				_pa.pa_id    = device.vendor_id() | device.device_id() << 16;

				if (probe_cfdata(&_pa)) {
					found++;
					break;
				}
			}

			return found;
		}

		/**************************
		 ** Bus_driver interface **
		 **************************/

		Genode::Irq_session_capability irq_session() override {
			return Platform::Device_client(_cap).irq(0); }

		Genode::addr_t alloc(Genode::size_t size, int align) override {
			return _dma_region_manager.alloc(size, align); }

		void free(Genode::addr_t virt, Genode::size_t size) override {
			_dma_region_manager.free(virt, size); }

		Genode::addr_t virt_to_phys(Genode::addr_t virt) override {
			return _dma_region_manager.virt_to_phys(virt); }

		Genode::addr_t phys_to_virt(Genode::addr_t phys) override {
			return _dma_region_manager.phys_to_virt(phys); }
};


/**********************
 ** Bus space helper **
 **********************/

struct Bus_space
{
	virtual unsigned read_1(unsigned long address) = 0;
	virtual unsigned read_2(unsigned long address) = 0;
	virtual unsigned read_4(unsigned long address) = 0;

	virtual void write_1(unsigned long address, unsigned char  value) = 0;
	virtual void write_2(unsigned long address, unsigned short value) = 0;
	virtual void write_4(unsigned long address, unsigned int   value) = 0;
};


/*********************
 ** I/O port helper **
 *********************/

struct Io_port : public Bus_space
{
	Genode::Io_port_session_client _io;
	Genode::addr_t                 _base;

	Io_port(Genode::addr_t base, Genode::Io_port_session_capability cap)
	: _io(cap), _base(base) { }

	unsigned read_1(unsigned long address) {
		return _io.inb(_base + address); }

	unsigned read_2(unsigned long address) {
		return _io.inw(_base + address); }

	unsigned read_4(unsigned long address) {
		return _io.inl(_base + address); }

	void write_1(unsigned long address, unsigned char value) {
		_io.outb(_base + address, value); }

	void write_2(unsigned long address, unsigned short value) {
		_io.outw(_base + address, value); }

	void write_4(unsigned long address, unsigned int value) {
		_io.outl(_base + address, value); }
};


/***********************
 ** I/O memory helper **
 ***********************/

struct Io_memory : public Bus_space
{
	Genode::Io_mem_session_client       _mem;
	Genode::Io_mem_dataspace_capability _mem_ds;
	Genode::addr_t                      _vaddr;

	Io_memory(Genode::Region_map                &rm,
	          Genode::addr_t                     base,
	          Genode::Io_mem_session_capability  cap)
	:
		_mem(cap),
		_mem_ds(_mem.dataspace())
	{
		if (!_mem_ds.valid())
			throw Genode::Exception();

		_vaddr = rm.attach(_mem_ds);
		_vaddr |= base & 0xfff;
	}

	unsigned read_1(unsigned long address) {
		return *(volatile unsigned char*)(_vaddr + address); }

	unsigned read_2(unsigned long address) {
		return *(volatile unsigned short*)(_vaddr + address); }

	unsigned read_4(unsigned long address) {
		return *(volatile unsigned int*)(_vaddr + address); }

	void write_1(unsigned long address, unsigned char value) {
		*(volatile unsigned char*)(_vaddr + address) = value; }

	void write_2(unsigned long address, unsigned short value) {
		*(volatile unsigned short*)(_vaddr + address) = value; }

	void write_4(unsigned long address, unsigned int value) {
		*(volatile unsigned int*)(_vaddr + address) = value; }
};

} /* anonymous namespace */


int Bsd::probe_drivers(Genode::Env &env, Genode::Allocator &alloc)
{
	Genode::log("--- probe drivers ---");
	static Pci_driver drv(env, alloc);
	return drv.probe();
}


/**********************
 ** dev/pci/pcivar.h **
 **********************/

extern "C" int pci_matchbyid(struct pci_attach_args *pa, const struct pci_matchid *ids, int num)
{
	pci_vendor_id_t  vid = PCI_VENDOR(pa->pa_id);
	pci_product_id_t pid = PCI_PRODUCT(pa->pa_id);

	for (int i = 0; i < num; i++) {
		if (vid == ids[i].pm_vid && pid == ids[i].pm_pid)
			return 1;
	}

	return 0;
}


extern "C" int pci_mapreg_map(struct pci_attach_args *pa,
                              int reg, pcireg_t type,
                              int flags, bus_space_tag_t *tagp,
                              bus_space_handle_t *handlep, bus_addr_t *basep,
                              bus_size_t *sizep, bus_size_t maxsize)
{
	/* calculate BAR from given register */
	int r = (reg - 0x10) / 4;

	Pci_driver *drv = (Pci_driver*)pa->pa_pc;

	Platform::Device_capability cap = drv->cap();
	Platform::Device_client device(cap);
	Platform::Device::Resource res = device.resource(r);

	switch (res.type()) {
	case Platform::Device::Resource::IO:
		{
			Io_port *iop = new (&drv->alloc())
			                   Io_port(res.base(), device.io_port(r));
			*tagp = (Genode::addr_t) iop;
			break;
		}
	case Platform::Device::Resource::MEMORY:
		{
			Io_memory *iom = new (&drv->alloc())
			                     Io_memory(drv->env().rm(), res.base(), device.io_mem(r));
			*tagp = (Genode::addr_t) iom;
			break;
		}
	case Platform::Device::Resource::INVALID:
		{
			Genode::error("PCI resource type invalid");
			return -1;
		}
	}

	*handlep = res.base();

	if (basep != 0)
		*basep = res.base();
	if (sizep != 0)
		*sizep = maxsize > 0 && res.size() > maxsize ? maxsize : res.size();

	/* enable bus master and I/O or memory bits */
	uint16_t cmd = device.config_read(Pci_driver::CMD, Platform::Device::ACCESS_16BIT);
	if (res.type() == Platform::Device::Resource::IO) {
		cmd &= ~Pci_driver:: CMD_MEMORY;
		cmd |= Pci_driver::CMD_IO;
	} else {
		cmd &= ~Pci_driver::CMD_IO;
		cmd |= Pci_driver::CMD_MEMORY;
	}

	cmd |= Pci_driver::CMD_MASTER;

	drv->pci().with_upgrade([&] () {
		device.config_write(Pci_driver::CMD, cmd, Platform::Device::ACCESS_16BIT);
	});

	return 0;
}


/***************************
 ** machine/pci_machdep.h **
 ***************************/

extern "C" pcireg_t pci_conf_read(pci_chipset_tag_t pc, pcitag_t tag, int reg)
{
	Pci_driver *drv = (Pci_driver *)pc;
	Platform::Device_client device(drv->cap());
	return device.config_read(reg, Platform::Device::ACCESS_32BIT);
}


extern "C" void pci_conf_write(pci_chipset_tag_t pc, pcitag_t tag, int reg,
                               pcireg_t val)
{
	Pci_driver *drv = (Pci_driver *)pc;
	Platform::Device_client device(drv->cap());
	return device.config_write(reg, val, Platform::Device::ACCESS_32BIT);
}


/*******************
 ** machine/bus.h **
 *******************/

extern "C" u_int8_t bus_space_read_1(bus_space_tag_t space,
                                     bus_space_handle_t handle,
                                     bus_size_t offset)
{
	Bus_space *bus = (Bus_space*)space;
	return bus->read_1(offset);
}


extern "C" u_int16_t bus_space_read_2(bus_space_tag_t space,
                                      bus_space_handle_t handle,
                                      bus_size_t offset)
{
	Bus_space *bus = (Bus_space*)space;
	return bus->read_2(offset);
}


extern "C" u_int32_t bus_space_read_4(bus_space_tag_t space,
                                      bus_space_handle_t handle,
                                      bus_size_t offset)
{
	Bus_space *bus = (Bus_space*)space;
	return bus->read_4(offset);
}


extern "C" void bus_space_write_1(bus_space_tag_t space,
                                  bus_space_handle_t handle,
                                  bus_size_t offset, u_int8_t value)
{
	Bus_space *bus = (Bus_space*)space;
	bus->write_1(offset, value);
}


extern "C" void bus_space_write_2(bus_space_tag_t space,
                                  bus_space_handle_t handle,
                                  bus_size_t offset, u_int16_t value)
{
	Bus_space *bus = (Bus_space*)space;
	bus->write_2(offset, value);
}


extern "C" void bus_space_write_4(bus_space_tag_t space,
                                  bus_space_handle_t handle,
                                  bus_size_t offset, u_int32_t value)
{
	Bus_space *bus = (Bus_space*)space;
	bus->write_4(offset, value);
}


extern "C" int bus_dmamap_create(bus_dma_tag_t tag, bus_size_t size, int nsegments,
                                 bus_size_t maxsegsz, bus_size_t boundart,
                                 int flags, bus_dmamap_t *dmamp)
{
	struct bus_dmamap *map;
	map = (struct bus_dmamap*) malloc(sizeof(struct bus_dmamap), M_DEVBUF, M_ZERO);

	map->size      = size;
	map->maxsegsz  = maxsegsz;
	map->nsegments = nsegments;

	*dmamp = map;

	return 0;
}


extern "C" void bus_dmamap_destroy(bus_dma_tag_t tag, bus_dmamap_t map) {
	free(map, 0, 0); }


extern "C" int bus_dmamap_load(bus_dma_tag_t tag, bus_dmamap_t dmam, void *buf,
                        bus_size_t buflen, struct proc *p, int flags)
{
	Bsd::Bus_driver *drv = (Bsd::Bus_driver *)tag;

	Genode::addr_t virt      = (Genode::addr_t)buf;
	dmam->dm_segs[0].ds_addr = drv->virt_to_phys(virt);

	return 0;
}


extern "C" void bus_dmamap_unload(bus_dma_tag_t, bus_dmamap_t)
{
	Genode::warning("not implemented, called from ",
	                __builtin_return_address(0));
}


extern "C" int bus_dmamem_alloc(bus_dma_tag_t tag, bus_size_t size, bus_size_t alignment,
                                bus_size_t boundary, bus_dma_segment_t *segs, int nsegs,
                                int *rsegs, int flags)
{
	Bsd::Bus_driver *drv = (Bsd::Bus_driver *)tag;

	Genode::addr_t virt = drv->alloc(size, Genode::log2(alignment));
	if (virt == 0)
		return -1;

	segs->ds_addr = drv->virt_to_phys(virt);
	segs->ds_size = size;
	*rsegs        = 1;

	return 0;
}


extern "C" void bus_dmamem_free(bus_dma_tag_t tag, bus_dma_segment_t *segs, int nsegs)
{
	Bsd::Bus_driver *drv = (Bsd::Bus_driver *)tag;

	for (int i = 0; i < nsegs; i++) {
		Genode::addr_t phys = (Genode::addr_t)segs[i].ds_addr;
		Genode::addr_t virt = drv->phys_to_virt(phys);

		drv->free(virt, segs[i].ds_size);
	}
}


extern "C" int bus_dmamem_map(bus_dma_tag_t tag, bus_dma_segment_t *segs, int nsegs,
                              size_t size, caddr_t *kvap, int flags)
{
	if (nsegs > 1) {
		Genode::error(__func__, ": cannot map more than 1 segment");
		return -1;
	}

	Bsd::Bus_driver *drv = (Bsd::Bus_driver *)tag;

	Genode::addr_t phys = (Genode::addr_t)segs[0].ds_addr;
	Genode::addr_t virt = drv->phys_to_virt(phys);

	*kvap = (caddr_t)virt;

	return 0;
}


extern "C" void bus_dmamem_unmap(bus_dma_tag_t, caddr_t, size_t) { }


extern "C" paddr_t bus_dmamem_mmap(bus_dma_tag_t, bus_dma_segment_t *,
                                   int, off_t, int, int)
{
	Genode::warning("not implemented, called from ",
	                __builtin_return_address(0));
	return 0;
}
