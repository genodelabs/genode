/*
 * \brief  ARM specific implemenations used on all SOCs
 * \author Stefan Kalkowski
 * \date   2020-08-18
 */

/*
 * Copyright (C) 2020 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

#include <platform_session/device.h>
#include <platform.h>

#include <lx_emul.h>
#include <legacy/lx_kit/backend_alloc.h>
#include <legacy/lx_kit/irq.h>
#include <legacy/lx_kit/malloc.h>

using namespace Genode;

struct Io_mem : List<Io_mem>::Element
{
	static addr_t io_mem_start;

	Io_mem_dataspace_capability       cap;
	addr_t                            start;
	size_t                            size;
	Constructible<Attached_dataspace> ds {};

	Io_mem(Io_mem_dataspace_capability cap,
	       addr_t                      off,
	       size_t                      size,
	       List<Io_mem>              & list)
	: cap(cap), start(io_mem_start+off), size(size)
	{
		io_mem_start += align_addr(off+size+PAGE_SIZE, PAGE_SHIFT);
		list.insert(this);
	}
};


struct Irq : List<Irq>::Element
{
	static unsigned irq_start;

	Irq_session_capability cap;
	unsigned               nr { irq_start++ };

	Irq(Irq_session_capability cap, List<Irq> & list)
	: cap(cap) { list.insert(this); }
};


struct Resource_env
{
	Platform::Connection platform;
	List<Io_mem> io_mem_list;
	List<Irq>    irq_list;

	Resource_env(Env & env) : platform(env) { };
};


enum {
	MAX_RESOURCES = 64,
	IO_MEM_START  = 0xf0000,
	IRQ_START     = 32,
};


addr_t   Io_mem::io_mem_start = IO_MEM_START;
unsigned Irq::irq_start       = IRQ_START;


static Resource_env & resource_env(Genode::Env * env = nullptr)
{
	static Resource_env r_env { *env };
	return r_env;
}


void lx_platform_device_init()
{
	using String = String<64>;
	using Device = Platform::Device;

	unsigned p_id = 0;

	resource_env().platform.with_xml([&] (Xml_node & xml)
	{
		xml.for_each_sub_node("device", [&] (Xml_node node)
		{
			Device::Name name = node.attribute_value("name", Device::Name());
			Device::Name type = node.attribute_value("type", Device::Name());

			using Platform::Device_interface;

			Capability<Device_interface> device_cap =
				resource_env().platform.acquire_device(name);

			platform_device *pdev = (platform_device *)kzalloc(sizeof(platform_device), 0);
			pdev->name = (char *)kzalloc(64,0);
			copy_cstring((char*)pdev->name, name.string(), 64);
			pdev->id = p_id++;

			unsigned res_count = 0;
			pdev->resource = (resource*) kzalloc(MAX_RESOURCES*sizeof(resource), 0);

			node.for_each_sub_node("io_mem", [&] (Xml_node node)
			{
				if (res_count >= MAX_RESOURCES)
					return;

				Device_interface::Range range { };
				Io_mem_session_client io_mem_client {
					device_cap.call<Device_interface::Rpc_io_mem>(res_count, range) };

				Io_mem & iom = *new (Lx_kit::env().heap())
					Io_mem(io_mem_client.dataspace(), range.start, range.size,
					       resource_env().io_mem_list);

				pdev->resource[res_count++] = { iom.start, iom.start+iom.size-1,
				                                "io_mem", IORESOURCE_MEM };
			});

			pdev->num_resources = res_count;
			res_count = 0;

			node.for_each_sub_node("irq", [&] (Xml_node node)
			{
				if (res_count+pdev->num_resources >= MAX_RESOURCES)
					return;

				Irq_session_capability irq_cap =
					device_cap.call<Device_interface::Rpc_irq>(res_count);

				Irq & irq = *new (Lx_kit::env().heap())
					Irq(irq_cap, resource_env().irq_list);

				pdev->resource[pdev->num_resources+res_count++] =
					{ irq.nr, irq.nr, "irq", IORESOURCE_IRQ };
			});

			pdev->num_resources += res_count;

			pdev->dev.of_node = (device_node*)kzalloc(sizeof(device_node), 0);
			pdev->dev.of_node->dev = &pdev->dev;
			property ** prop = &pdev->dev.of_node->properties;

			*prop           = (property*) kzalloc(sizeof(property), 0);
			(*prop)->name   = "compatible";
			(*prop)->value  = kzalloc(64,0);
			copy_cstring((char*)(*prop)->value, type.string(), 64);
			prop            = &(*prop)->next;

			node.for_each_sub_node("property", [&] (Xml_node node) {
				*prop           = (property*) kzalloc(sizeof(property), 0);
				(*prop)->name   = (char*)kzalloc(64,0);
				(*prop)->value  = kzalloc(64,0);
				copy_cstring((char*)(*prop)->name,  node.attribute_value("name",  String()).string(), 64);
				copy_cstring((char*)(*prop)->value, node.attribute_value("value", String()).string(), 64);
				prop = &(*prop)->next;
			});

			/*
			 * Needed for DMA buffer allocation. See 'hcd_buffer_alloc' in 'buffer.c'
			 */
			static u64 dma_mask = ~(u64)0;
			pdev->dev.dma_mask = &dma_mask;
			pdev->dev.coherent_dma_mask = ~0;

			platform_device_register(pdev);
		});
	});

	resource_env().platform.update();
}


/****************************
 ** lx_kit/backend_alloc.h **
 ****************************/

void backend_alloc_init(Env & env, Ram_allocator&, Allocator&)
{
	resource_env(&env);
}


Ram_dataspace_capability Lx::backend_alloc(addr_t size, Cache cache)
{
	return resource_env().platform.alloc_dma_buffer(size, cache);
}


void Lx::backend_free(Ram_dataspace_capability cap)
{
	return resource_env().platform.free_dma_buffer(cap);
}


Genode::addr_t Lx::backend_dma_addr(Genode::Ram_dataspace_capability cap)
{
	return resource_env().platform.dma_addr(cap);
}


/**********************
 ** asm-generic/io.h **
 **********************/

void * _ioremap(phys_addr_t phys_addr, unsigned long size, int)
{
	for (Io_mem * iom = resource_env().io_mem_list.first(); iom; iom = iom->next()) {
		if (iom->start <= phys_addr && iom->start+iom->size >= phys_addr+size) {
			iom->ds.construct(Lx_kit::env().env().rm(), iom->cap);
			addr_t off = phys_addr - iom->start;
			off       += iom->start & (addr_t)(PAGE_SIZE-1);
			return (void*)((addr_t)iom->ds->local_addr<void>() + off);
		}
	}

	warning("did not found physical resource ", (void*)phys_addr);
	return nullptr;
}


void *ioremap(phys_addr_t offset, unsigned long size)
{
	return _ioremap(offset, size, 0);
}


/***********************
 ** linux/interrupt.h **
 ***********************/

extern "C" int request_irq(unsigned int irq, irq_handler_t handler, unsigned long flags,
                           const char *name, void *dev)
{
	for (Irq * i = resource_env().irq_list.first(); i; i = i->next()) {
		if (i->nr != irq) { continue; }
		Lx::Irq::irq().request_irq(i->cap, irq, handler, dev);
	}

	return 0;
}


int devm_request_irq(struct device *dev, unsigned int irq, irq_handler_t handler, unsigned long irqflags, const char *devname, void *dev_id)
{
	return request_irq(irq, handler, irqflags, devname, dev_id);
}
