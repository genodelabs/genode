/*
 * \brief  ACPI device information from config
 * \author Christian Helmuth
 * \date   2022-05-16
 */

/*
 * Copyright (C) 2022 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <base/allocator.h>
#include <base/registry.h>
#include <util/xml_node.h>
#include <util/attempt.h>


namespace Platform { namespace Acpi {
	using namespace Genode;

	class Device;

	class Device_registry;
} }


struct Platform::Acpi::Device : Interface
{
	typedef String<10> Hid; /* ACPI Spec 6.1.5 Hardware ID */

	struct Resource
	{
		enum class Type { IRQ, IOMEM, IOPORT };

		enum class Irq { EDGE, LEVEL_LOW, LEVEL_HIGH };

		Type   type;
		addr_t base;

		union {
			size_t size;
			Irq    irq;
		};

		void print(Output &o) const;
	};

	enum struct Invalid_resource { };
	typedef Attempt<Resource, Invalid_resource> Resource_result;

	virtual Hid hid() const = 0;
	virtual Resource_result resource(unsigned idx) const = 0;
	virtual Resource_result irq(unsigned id) const = 0;
	virtual Resource_result iomem(unsigned id) const = 0;
	virtual Resource_result ioport(unsigned id) const = 0;
};


struct Platform::Acpi::Device_registry : Genode::Registry<Device>
{
	bool _initialized { false };

	void init_devices(Allocator &heap, Xml_node config);

	enum struct Lookup_failed { };
	typedef Attempt<Device const *, Lookup_failed> Lookup_result;

	Lookup_result lookup(Device::Hid name) const;
};
