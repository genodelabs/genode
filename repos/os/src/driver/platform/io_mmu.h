/*
 * \brief  Platform driver - IO MMU interface
 * \author Johannes Schlatow
 * \date   2023-01-20
 */

/*
 * Copyright (C) 2023 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _SRC__DRIVER__PLATFORM__IO_MMU_H_
#define _SRC__DRIVER__PLATFORM__IO_MMU_H_

/* Genode includes */
#include <base/registry.h>
#include <base/quota_guard.h>
#include <pci/types.h>
#include <platform_session/platform_session.h>

/* local includes */
#include <types.h>
#include <dma_allocator.h>
#include <irq_controller.h>

namespace Driver
{
	using namespace Genode;

	class Device;
	class Device_model;
	class Io_mmu;
	class Io_mmu_factory;

	using Io_mmu_devices = Registry<Io_mmu>;
}


class Driver::Io_mmu : private Io_mmu_devices::Element
{
	public:

		using Range      = Platform::Device_interface::Range;
		using Irq_config = Irq_controller::Irq_config;

		struct Irq_info {
			enum { DIRECT, REMAPPED } remapped;
			Irq_session::Info         session_info;
			unsigned                  irq_number;
		};

		class Domain
		{
			private:

				friend class Io_mmu;

				Allocator &_md_alloc;

			public:

				Allocator & md_alloc() { return _md_alloc; }

				/* interface for adding/removing DMA buffers */
				virtual void add_range(Range const &, addr_t const,
				                       Dataspace_capability const) {};
				virtual void remove_range(Range const &) {};

				Domain(Allocator &md_alloc) : _md_alloc(md_alloc) { }

				virtual ~Domain() { }
		};

	protected:

		friend class Domain;

		Device_name _name;

		unsigned _active_domains { 0 };

		virtual void _enable()  { };
		virtual void _disable() { };

		void _enable_domain()
		{
			if (!_active_domains)
				_enable();

			_active_domains++;
		}

		void _disable_domain()
		{
			if (_active_domains > 0)
				_active_domains--;

			if (!_active_domains)
				_disable();
		}

	public:

		/* suspend/resume interface */
		virtual void suspend() { }
		virtual void resume() { }

		/* interface for mapping/unmapping interrupts */
		virtual void     unmap_irq(Pci::Bdf const &, unsigned) { }
		virtual Irq_info map_irq(Pci::Bdf const &, Irq_info const &info, Irq_config const &) {
			return info; }

		Device_name const & name() const { return _name; }

		/* Return true if device requires physical addressing */
		virtual bool mpu() const { return false; }

		/* Create a Io_mmu::Domain object */
		virtual Domain & create_domain(Allocator&, Ram_allocator&,
		                               Ram_quota_guard&, Cap_quota_guard&) = 0;
		virtual void destroy_domain(Allocator &, Domain &) = 0;

		virtual void enregister(Device const &, Domain &) {};
		virtual void deregister(Device const &, Domain &) {};

		virtual void iotlb_flush(Domain &) {};

		virtual void generate(Generator &) const { }

		Io_mmu(Io_mmu_devices &io_mmu_devices, Device_name const &name)
		: Io_mmu_devices::Element(io_mmu_devices, *this),
			_name(name)
		{ }

		virtual ~Io_mmu() { }
};


class Driver::Io_mmu_factory : private Genode::Registry<Io_mmu_factory>::Element
{
	protected:

		Device_type _type;

	public:

		Io_mmu_factory(Registry<Io_mmu_factory> &registry, Device_type const &type)
		: Registry<Io_mmu_factory>::Element(registry, *this),
		  _type(type)
		{ }

		virtual ~Io_mmu_factory() { }

		bool matches(Device const &dev);

		virtual void create(Allocator &, Io_mmu_devices &io_mmu_devices,
		                    Device const &, Device_model &) = 0;
};


#endif /* _SRC__DRIVER__PLATFORM__IO_MMU_H_ */
