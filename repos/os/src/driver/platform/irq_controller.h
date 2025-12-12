/*
 * \brief  Platform driver - IRQ controller interface
 * \author Johannes Schlatow
 * \date   2024-03-20
 */

/*
 * Copyright (C) 2024 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _SRC__DRIVER__PLATFORM__IRQ_CONTROLLER_H_
#define _SRC__DRIVER__PLATFORM__IRQ_CONTROLLER_H_

/* Genode includes */
#include <base/registry.h>
#include <pci/types.h>

/* local includes */
#include <types.h>

namespace Driver
{
	using namespace Genode;

	class Device;
	class Irq_controller;
	class Irq_controller_factory;
}


class Driver::Irq_controller : private Registry<Irq_controller>::Element
{
	private:

		Device_name _name;
		Device_name _iommu_name;
		Pci::Bdf     _bdf;

	public:

		struct Irq_config
		{
			enum Mode { INVALID,
			            PHYSICAL,
			            LOGICAL } mode;
			Irq_session::Trigger  trigger;
			unsigned              vector;
			unsigned              destination;

			static Irq_config Invalid() {
				return { Mode::INVALID, Irq_session::TRIGGER_UNCHANGED, 0, 0 }; }
		};

		Device_name const &name()  const { return _name; }
		Device_name const &iommu() const { return _iommu_name; }
		Pci::Bdf    const &bdf()   const { return _bdf;}

		virtual void remap_irq(unsigned from, unsigned to) = 0;
		virtual bool handles_irq(unsigned) = 0;

		virtual Irq_config irq_config(unsigned) = 0;

		Irq_controller(Registry<Irq_controller>  &registry,
		               Device_name         const &name,
		               Device_name         const &iommu_name,
		               Pci::Bdf            const &bdf)
		: Registry<Irq_controller>::Element(registry, *this),
		  _name(name), _iommu_name(iommu_name), _bdf(bdf)
		{ }

		virtual ~Irq_controller() { }
};


class Driver::Irq_controller_factory : private Genode::Registry<Irq_controller_factory>::Element
{
	protected:

		Device_type  _type;

	public:

		Irq_controller_factory(Registry<Irq_controller_factory> &registry,
		                       Device_type                const &type)
		: Registry<Irq_controller_factory>::Element(registry, *this),
		  _type(type)
		{ }

		virtual ~Irq_controller_factory() { }

		bool matches(Device const &dev);

		virtual void create(Allocator &,
		                    Registry<Irq_controller> &,
		                    Device const &) = 0;
};


#endif /* _SRC__DRIVER__PLATFORM__IRQ_CONTROLLER_H_ */
