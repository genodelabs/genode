/*
 * \brief  Lx_emul support for ACPI devices
 * \author Christian Helmuth
 * \date   2022-05-20
 */

/*
 * Copyright (C) 2022 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

#include <base/registry.h>

#include <lx_kit/env.h>
#include <lx_emul/acpi.h>


using namespace Lx_kit;

namespace Lx_kit { namespace Acpi {
	typedef String<16> Name;

	struct Resource_array;
	struct Device;

	Registry<Device> *registry;
} }


struct Acpi::Resource_array : Noncopyable
{
	/*
	 * Noncopyable
	 */
	Resource_array(Resource_array const &) = delete;
	Resource_array &operator = (Resource_array const &) = delete;

	Heap &heap;

	unsigned max { 0 };
	unsigned num { 0 };

	lx_emul_acpi_resource *res { nullptr };

	Resource_array(Heap &heap) : heap(heap) { }

	~Resource_array()
	{
		if (res)
			heap.free((void *)res, 0);
	}

	void add(lx_emul_acpi_resource_type type, unsigned long base, unsigned long size)
	{
		/* grow resource array on first addition and if full */
		if (num == max) {
			unsigned               old_max = max;
			lx_emul_acpi_resource *old     = res;

			max += 1;
			res = (lx_emul_acpi_resource *)heap.alloc(max*sizeof(*res));

			if (old) {
				memcpy(res, old, old_max*sizeof(*res));
				heap.free((void *)old, 0);
			}
		}

		res[num++] = { type, base, size };
	}
};


struct Acpi::Device : Registry<Device>::Element, Noncopyable
{
	/*
	 * Noncopyable
	 */
	Device(Device const &) = delete;
	Device &operator = (Device const &) = delete;

	Name   const name;
	void * const handle;

	void *device = nullptr; /* used by scan.c */
	void *object = nullptr; /* used by acpixf.c */

	Resource_array resources;

	Device(Registry<Device> &registry, Heap &heap, Name name, void *handle)
	:
		Registry<Device>::Element(registry, *this),
		name(name), handle(handle), resources(heap)
	{
		log("new Acpi::Device name=", name, " handle=", handle);
	}
};


template <typename FN>
static void with_device(void *handle, FN const &fn)
{
	if (!Acpi::registry) {
		error("lx_emul: ACPI device registry not initialized");
		return;
	}

	Acpi::registry->for_each([&] (Acpi::Device &d) {
		if (d.handle == handle)
			fn(d);
	});
}


char const * lx_emul_acpi_name(void *handle)
{
	char const *result = nullptr;

	with_device(handle, [&] (Acpi::Device const &d) {
		result = d.name.string(); });

	return result;
}


void * lx_emul_acpi_device(void *handle)
{
	void *result = nullptr;

	with_device(handle, [&] (Acpi::Device const &d) {
		result = d.device; });

	return result;
}


void * lx_emul_acpi_object(void *handle)
{
	void *result = nullptr;

	with_device(handle, [&] (Acpi::Device const &d) {
		result = d.object; });

	return result;
}


void lx_emul_acpi_set_device(void *handle, void *device)
{
	with_device(handle, [&] (Acpi::Device &d) {
		d.device = device; });
}


void lx_emul_acpi_set_object(void *handle, void *object)
{
	with_device(handle, [&] (Acpi::Device &d) {
		d.object = object; });
}


void lx_emul_acpi_for_each_device(lx_emul_acpi_device_callback cb, void *context)
{
	if (!Acpi::registry) {
		error("lx_emul: ACPI device registry not initialized");
		return;
	}

	Acpi::registry->for_each([&] (Acpi::Device &d) {
		void *device =
			cb(d.handle, d.name.string(), d.resources.num, d.resources.res, context);

		if (device)
			d.device = device;
	});
}


void lx_emul_acpi_init()
{
	unsigned long handle = 1;

	Acpi::registry = new (env().heap) Registry<Acpi::Device>();

	env().devices.for_each([&] (Device &d) {

		/* FIXME create PCI companion devices here ? */
		if (String<8>("acpi") != d.compatible())
			return;

		auto &device =
			*new (env().heap) Acpi::Device(*Acpi::registry, env().heap,
			                               d.name(), (void *)handle++);

		d.for_each_io_mem([&] (Lx_kit::Device::Io_mem &m) {
			device.resources.add(LX_EMUL_ACPI_IOMEM, m.addr, m.size); });
		d.for_each_io_port([&] (Lx_kit::Device::Io_port &p) {
			device.resources.add(LX_EMUL_ACPI_IOPORT, p.addr, p.size); });
		d.for_each_irq([&] (Lx_kit::Device::Irq &i) {
			device.resources.add(LX_EMUL_ACPI_IRQ, i.number, 1); });
	});

	/* FIXME add locally configured ACPI devices (e.g., touchpad "EPTP" */
	new (env().heap) Acpi::Device(*Acpi::registry, env().heap,
	                              "I2C3", (void *)handle++);
	new (env().heap) Acpi::Device(*Acpi::registry, env().heap,
	                              "EPTP", (void *)handle++);
}
