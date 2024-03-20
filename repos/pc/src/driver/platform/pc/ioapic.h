/*
 * \brief  IOAPIC implementation
 * \author Johannes Schlatow
 * \date   2024-03-20
 */

/*
 * Copyright (C) 2024 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _SRC__DRIVERS__PLATFORM__PC__IOAPIC_H_
#define _SRC__DRIVERS__PLATFORM__PC__IOAPIC_H_

/* Genode includes */
#include <os/attached_mmio.h>
#include <util/register_set.h>
#include <base/allocator.h>

/* Platform-driver includes */
#include <device.h>
#include <irq_controller.h>

namespace Driver {
	using namespace Genode;

	class Ioapic;
	class Ioapic_factory;
}


class Driver::Ioapic : private Attached_mmio<0x1000>,
                       public  Driver::Irq_controller
{
	private:

		Env          & _env;
		unsigned       _irq_start;
		unsigned       _max_entries;

		/**
		 * Registers
		 */

		struct Ioregsel : Register<0x00, 32> {
			enum {
				IOAPICVER = 0x01,
				IOREDTBL  = 0x10
			};
		};

		struct Iowin    : Register<0x10, 32> {
			struct Maximum_entries : Bitfield<16, 8> { };
		};

		struct Irte : Genode::Register<64> {
			struct Index_15   : Bitfield<11, 1> { };
			struct Remap      : Bitfield<48, 1>  { };
			struct Index_0_14 : Bitfield<49, 15> { };
			struct Index      : Bitset_2<Index_0_14, Index_15> { };

			struct Vector       : Bitfield<0,8> { };
			struct Trigger_mode : Bitfield<15,1> {
				enum { EDGE = 0, LEVEL = 1 }; };

			struct Destination_mode : Bitfield<11,1> {
				enum { PHYSICAL = 0, LOGICAL = 1 }; };

			struct Destination : Bitfield<56,8> { };
		};

		unsigned _read_max_entries();

	public:

		/**
		 * Irq_controller interface
		 */
		void       remap_irq(unsigned, unsigned) override;
		bool       handles_irq(unsigned) override;
		Irq_config irq_config(unsigned) override;

		/**
		 * Constructor/Destructor
		 */

		Ioapic(Env                      & env,
		       Registry<Irq_controller> & irq_controller_registry,
		       Device::Name       const & name,
		       Device::Name       const & iommu_name,
		       Pci::Bdf           const & bdf,
		       Device::Io_mem::Range      range,
		       unsigned                   irq_start)
		: Attached_mmio(env, {(char *)range.start, range.size}),
		  Driver::Irq_controller(irq_controller_registry, name, iommu_name, bdf),
		  _env(env), _irq_start(irq_start), _max_entries(_read_max_entries())
		{ }
};


class Driver::Ioapic_factory : public Driver::Irq_controller_factory
{
	private:

		Genode::Env  & _env;

	public:

		Ioapic_factory(Genode::Env & env, Registry<Irq_controller_factory> & registry)
		: Driver::Irq_controller_factory(registry, Device::Type { "ioapic" }),
		  _env(env)
		{ }

		void create(Allocator & alloc, Registry<Irq_controller> & irq_controller_registry, Device const & device) override
		{
			using Range    = Device::Io_mem::Range;
			using Property = Device::Property;

			/* evaluate properties (remapping support, base IRQ, routing id) */
			bool       remap     { false };
			unsigned   irq_start { 0 };
			Pci::rid_t rid       { 0 };
			device.for_each_property([&] (Property::Name const & name, Property::Value const & value) {
				if (name == "remapping")
					ascii_to(value.string(), remap);
				else if (name == "irq_start")
					ascii_to(value.string(), irq_start);
				else if (name == "routing_id")
					ascii_to(value.string(), rid);
			});

			/* ignore IOAPIC devices without remapping support */
			if (!remap)
				return;

			unsigned iommu_idx = 0;
			device.for_each_io_mmu([&] (Device::Io_mmu const & iommu) {
				if (iommu_idx++) return;

				device.for_each_io_mem([&] (unsigned idx, Range range, Device::Pci_bar, bool)
				{
					if (idx == 0)
						new (alloc) Ioapic(_env, irq_controller_registry, device.name(),
						                   iommu.name, Pci::Bdf::bdf(rid), range, irq_start);
				});
			}, [] () { /* empty fn */ });
		}
};

#endif /* _SRC__DRIVERS__PLATFORM__PC__IOAPIC_H_ */
