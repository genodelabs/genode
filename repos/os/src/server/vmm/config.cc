/*
 * \brief  VMM for ARM virtualization - config frontend
 * \author Stefan Kalkowski
 * \date   2022-11-10
 */

/*
 * Copyright (C) 2022 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <config.h>

using namespace Genode;
using namespace Vmm;


Config::Virtio_device::Virtio_device(Name & name, Type type, Config & config)
:
	_config(config),
	name(name),
	type(type),
	mmio_start(config._mmio_alloc.alloc(MMIO_SIZE)),
	mmio_size(MMIO_SIZE),
	irq(config._irq_alloc.alloc()) {}


Config::Virtio_device::~Virtio_device()
{
	_config._irq_alloc.free(irq);
	_config._mmio_alloc.free(mmio_start);
}


void Vmm::Config::update(Xml_node node)
{
	_kernel_name = node.attribute_value("kernel_rom",  Name("linux"));
	_initrd_name = node.attribute_value("initrd_rom",  Name());
	_ram_size    = node.attribute_value("ram_size",    Number_of_bytes());
	_cpu_count   = node.attribute_value("cpu_count",   0U);
	_cpu_type    = node.attribute_value("cpu_type",    Name("arm,cortex-a15"));
	_gic_version = node.attribute_value("gic_version", 2U);
	_bootargs    = node.attribute_value("bootargs",    Arguments("console=ttyAMA0"));

	if (_gic_version < 2 || _gic_version > 3) {
		error("Invalid GIC version, supported are: 2 and 3");
		throw Invalid_configuration();
	}

	if (_ram_size < MINIMUM_RAM_SIZE) {
		error("Minimum RAM size is ", Hex((size_t)MINIMUM_RAM_SIZE));
		warning("Reset RAM size to minimum");
		_ram_size = MINIMUM_RAM_SIZE;
	}

	if (_cpu_count < 1) {
		error("Minimum CPU count is 1");
		warning("Reset CPU count to minimum");
		_cpu_count = 1;
	}

	update_list_model_from_xml(_model, node,

		/* create */
		[&] (Xml_node const &node) -> Virtio_device &
		{
			Config::Name name = node.attribute_value("name", Config::Name());
			Virtio_device::Type t = Virtio_device::type_from_xml(node);

			if (t == Virtio_device::INVALID || !name.valid()) {
				error("Invalid type or missing name in Virtio device node");
				throw Invalid_configuration();
			}
			return *new (_heap) Virtio_device(name, t, *this);
		},

		/* destroy */
		[&] (Virtio_device &dev) { destroy(_heap, &dev); },

		/* update */
		[&] (Virtio_device &, Xml_node const &) { }
	);
}
