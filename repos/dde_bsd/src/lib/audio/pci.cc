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
#include <platform_session/device.h>
#include <platform_session/dma_buffer.h>
#include <util/retry.h>

/* local includes */
#include <audio/audio.h>
#include <bsd.h>
#include <bsd_emul.h>
#include <scheduler.h>

#include <extern_c_begin.h>
# include <dev/pci/pcidevs.h>
#include <extern_c_end.h>

static constexpr bool debug = false;

extern "C" int probe_cfdata(struct pci_attach_args *);

static void run_irq(void *args);

namespace {

class Pci_driver
{
	private:

		enum { DMA_SIZE = 256 * 1024 };

		Genode::Env          & _env;
		Platform::Connection   _pci    { _env };
		Platform::Dma_buffer   _buffer { _pci, DMA_SIZE, Genode::UNCACHED };
		Genode::Allocator_avl  _alloc;

		struct Device
		{
			Platform::Device       dev;
			Platform::Device::Irq  irq;
			Platform::Device::Mmio mmio;

			Device(Platform::Connection &pci,
			       Platform::Device::Name const &name)
			: dev { pci, name }, irq { dev }, mmio { dev }
			{ }
		};

		Genode::Constructible<Device> _device { };

		uint16_t _vendor_id     { 0U };
		uint16_t _device_id     { 0U };
		uint32_t _class_code    { 0U };
		uint16_t _sub_vendor_id { 0U };
		uint16_t _sub_device_id { 0U };

		typedef int (*intrh_t)(void*);

		intrh_t   _irq_func { nullptr };
		void    * _irq_arg  { nullptr };
		Bsd::Task _irq_task { run_irq, this, "irq", Bsd::Task::PRIORITY_3,
		                      Bsd::scheduler(), 1024 * sizeof(long) };

		Genode::Io_signal_handler<Pci_driver> _irq_handler { _env.ep(), *this,
		                                                     &Pci_driver::_irq_handle };

		struct pci_attach_args _pa { 0, 0, 0, 0, 0 };

		void _irq_handle()
		{
			_irq_task.unblock();
			Bsd::scheduler().schedule();
		}

		Genode::addr_t _buffer_base() {
			return (Genode::addr_t)_buffer.local_addr<char>(); }

		void _handle_device_list() { /* intentionally left empty */ }

		void _wait_for_device_list(Genode::Entrypoint &ep,
		                                  Platform::Connection &pci)
		{
			using namespace Genode;
			Constructible<Io_signal_handler<Pci_driver>> handler { };

			bool device_list = false;
			while (!device_list) {
				pci.update();
				pci.with_xml([&] (Xml_node & xml) {
					if (xml.num_sub_nodes()) {
						pci.sigh(Signal_context_capability());
						if (handler.constructed())
							handler.destruct();
						device_list = true;
						return;
					}

					if (!handler.constructed()) {
						handler.construct(ep, *this,
						                  &Pci_driver::_handle_device_list);
						pci.sigh(*handler);
					}

					ep.wait_and_dispatch_one_io_signal();
				});
			}
		}

	public:

		Pci_driver(Genode::Env &env, Genode::Allocator & alloc)
		:
			_env(env), _alloc(&alloc)
		{
			_alloc.add_range(_buffer_base(), DMA_SIZE);

			/* will "block" until device list becomes available */
			_wait_for_device_list(_env.ep(), _pci);
		}

		uint16_t sub_device_id() { return _sub_device_id; }
		uint16_t sub_vendor_id() { return _sub_vendor_id; }

		int probe()
		{
			using namespace Genode;

			_pci.upgrade_ram(8*1024);

			/*
			 * We hide ourself in the bus_dma_tag_t as well as
			 * in the pci_chipset_tag_t field because they are
			 * used in all pci or bus related functions and are
			 * our access window, hence.
			 */
			_pa.pa_dmat = (bus_dma_tag_t)this;
			_pa.pa_pc   = (pci_chipset_tag_t)this;

			bool found = false;
			_pci.update();
			_pci.with_xml([&] (Xml_node node) {
				node.for_each_sub_node("device", [&] (Xml_node node)
				{
					/* only use the first successfully probed device */
					if (found) return;

					String<16> name = node.attribute_value("name", String<16>());

					node.with_optional_sub_node("pci-config", [&] (Xml_node node)
					{
						_vendor_id      = node.attribute_value("vendor_id", 0U);
						_device_id      = node.attribute_value("device_id", 0U);
						_class_code     = node.attribute_value("class", 0U);
						_sub_vendor_id  = node.attribute_value("sub_vendor_id", 0U);
						_sub_device_id  = node.attribute_value("sub_device_id", 0U);

						if (_device.constructed())
							_device.destruct();

						_device.construct(_pci, name);
						_device->irq.sigh(_irq_handler);

						/* we do the shifting to match OpenBSD's assumptions */
						_pa.pa_tag   = 0x80000000UL;
						_pa.pa_class = _class_code << 8;
						_pa.pa_id    = _vendor_id | _device_id << 16;

						if (probe_cfdata(&_pa))
							found = true;
					});
				});
			});

			return found ? 1 : 0;
		}

		void irq_handler(intrh_t handler, void * arg)
		{
			_irq_func = handler;
			_irq_arg  = arg;
		}

		void handle_irq()
		{
			_irq_func(_irq_arg);
			_device->irq.ack();
		}


		/*********************
		 ** Mmio management **
		 *********************/

		Genode::addr_t mmio_base() { return _device->mmio.base(); }
		Genode::size_t mmio_size() { return _device->mmio.size(); }

		template <typename T>
		T read(Genode::size_t offset) {
			return *(volatile T*)(_device->mmio.base() + offset); }

		template <typename T>
		void write(Genode::size_t offset, T value) {
			*(volatile T*)(_device->mmio.base() + offset) = value; }


		/********************
		 ** DMA management **
		 ********************/

		Genode::addr_t alloc(Genode::size_t size, unsigned align)
		{
			using namespace Genode;

			return _alloc.alloc_aligned(size, align).convert<Genode::addr_t>(
				[&] (void *ptr)                  { return (addr_t)ptr; },
				[&] (Allocator_avl::Alloc_error) { return 0UL; });
		}

		void free(Genode::addr_t virt, Genode::size_t size) {
			_alloc.free((void*)virt, size); }

		Genode::addr_t virt_to_phys(Genode::addr_t virt) {
			return virt - _buffer_base() + _buffer.dma_addr(); }

		Genode::addr_t phys_to_virt(Genode::addr_t phys) {
			return phys - _buffer.dma_addr() + _buffer_base(); }
};


/**********************
 ** dev/pci/pcivar.h **
 **********************/

extern "C" int pci_intr_map(struct pci_attach_args *pa, pci_intr_handle_t *ih) {
	return 0; }


extern "C" void *pci_intr_establish(pci_chipset_tag_t pc, pci_intr_handle_t ih,
                                    int ipl, int (*intrh)(void *), void *intarg,
                                    const char *intrstr)
{
	Pci_driver * drv = (Pci_driver*) pc;
	drv->irq_handler(intrh, intarg);
	return drv;
}

} /* anonymous namespace */

static void run_irq(void *args)
{
	Pci_driver & pci_drv = *(Pci_driver*)args;

	while (true) {
		Bsd::scheduler().current()->block_and_schedule();
		pci_drv.handle_irq();
	}
}


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

	if (r) {
		Genode::error("MAP BAR ", r, " not implemented yet");
		return -1;
	}

	Pci_driver *drv = (Pci_driver*)pa->pa_pc;
	*tagp    = (Genode::addr_t)drv;
	*handlep = drv->mmio_base();

	if (basep != 0)
		*basep = drv->mmio_base();
	if (sizep != 0)
		*sizep = maxsize > 0 && drv->mmio_size() > maxsize ? maxsize : drv->mmio_size();
	return 0;
}


/***************************
 ** machine/pci_machdep.h **
 ***************************/

extern "C" pcireg_t pci_conf_read(pci_chipset_tag_t pc, pcitag_t tag, int reg)
{
	Pci_driver *drv = (Pci_driver *)pc;

	switch (reg) {
	case 0x4:  return 0x207; /* command register */
	case 0x10: return drv->mmio_base();
	case 0x2c: return drv->sub_device_id() << 16 | drv->sub_vendor_id();
	default:
		if (debug)
			Genode::warning("Ignore reading of PCI config space @ ", reg);
	};
	return 0;
}


extern "C" void pci_conf_write(pci_chipset_tag_t pc, pcitag_t tag, int reg,
                               pcireg_t val)
{
	if (debug)
		Genode::warning("Ignore writing of PCI config space @ ",
		                reg, " val=", val);
}


/*******************
 ** machine/bus.h **
 *******************/

extern "C" u_int8_t bus_space_read_1(bus_space_tag_t space,
                                     bus_space_handle_t handle,
                                     bus_size_t offset)
{
	return ((Pci_driver*)space)->read<Genode::uint8_t>(offset);
}


extern "C" u_int16_t bus_space_read_2(bus_space_tag_t space,
                                      bus_space_handle_t handle,
                                      bus_size_t offset)
{
	return ((Pci_driver*)space)->read<Genode::uint16_t>(offset);
}


extern "C" u_int32_t bus_space_read_4(bus_space_tag_t space,
                                      bus_space_handle_t handle,
                                      bus_size_t offset)
{
	return ((Pci_driver*)space)->read<Genode::uint32_t>(offset);
}


extern "C" void bus_space_write_1(bus_space_tag_t space,
                                  bus_space_handle_t handle,
                                  bus_size_t offset, u_int8_t value)
{
	((Pci_driver*)space)->write(offset, value);
}


extern "C" void bus_space_write_2(bus_space_tag_t space,
                                  bus_space_handle_t handle,
                                  bus_size_t offset, u_int16_t value)
{
	((Pci_driver*)space)->write(offset, value);
}


extern "C" void bus_space_write_4(bus_space_tag_t space,
                                  bus_space_handle_t handle,
                                  bus_size_t offset, u_int32_t value)
{
	((Pci_driver*)space)->write(offset, value);
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
	Pci_driver * drv = (Pci_driver*) tag;

	Genode::addr_t virt      = (Genode::addr_t)buf;
	dmam->dm_segs[0].ds_addr = drv->virt_to_phys(virt);

	return 0;
}


extern "C" int bus_dmamem_alloc(bus_dma_tag_t tag, bus_size_t size, bus_size_t alignment,
                                bus_size_t boundary, bus_dma_segment_t *segs, int nsegs,
                                int *rsegs, int flags)
{
	Pci_driver * drv = (Pci_driver*) tag;

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
	Pci_driver * drv = (Pci_driver*) tag;

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

	Pci_driver * drv = (Pci_driver*) tag;

	Genode::addr_t phys = (Genode::addr_t)segs[0].ds_addr;
	Genode::addr_t virt = drv->phys_to_virt(phys);

	*kvap = (caddr_t)virt;

	return 0;
}
