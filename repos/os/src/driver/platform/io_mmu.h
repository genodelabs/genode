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

#ifndef _SRC__DRIVERS__PLATFORM__IO_MMU_H_
#define _SRC__DRIVERS__PLATFORM__IO_MMU_H_

/* Genode includes */
#include <base/registry.h>
#include <base/quota_guard.h>
#include <pci/types.h>

/* local includes */
#include <device.h>
#include <dma_allocator.h>

namespace Driver
{
	using namespace Genode;

	class Io_mmu;
	class Io_mmu_factory;

	using Io_mmu_devices = Registry<Io_mmu>;
}


class Driver::Io_mmu : private Io_mmu_devices::Element
{
	public:

		using Range = Platform::Device_interface::Range;

		class Domain : private Registry<Domain>::Element
		{
			private:

				friend class Io_mmu;

				Io_mmu     & _io_mmu;
				Allocator  & _md_alloc;

				unsigned     _active_devices { 0 };

			public:

				Allocator & md_alloc() { return _md_alloc; }

				Device::Name const & device_name() const { return _io_mmu.name(); }

				void enable_device()
				{
					_active_devices++;

					if (_active_devices == 1)
						_io_mmu._enable_domain();
				}

				void disable_device()
				{
					if (_active_devices > 0) {
						_active_devices--;

						if (_active_devices == 0)
							_io_mmu._disable_domain();
					}
				}

				unsigned devices() const { return _active_devices; }

				/* interface for (un)assigning a pci device */
				virtual void enable_pci_device(Io_mem_dataspace_capability const,
				                               Pci::Bdf const &) = 0;
				virtual void disable_pci_device(Pci::Bdf const &) = 0;

				/* interface for adding/removing DMA buffers */
				virtual void add_range(Range const &,
				                       addr_t const,
				                       Dataspace_capability const) = 0;
				virtual void remove_range(Range const &) = 0;

				Domain(Io_mmu                     & io_mmu,
				       Allocator                  & md_alloc)
				: Registry<Domain>::Element(io_mmu._domains, *this),
				  _io_mmu(io_mmu), _md_alloc(md_alloc)
				{ }

				virtual ~Domain() { }
		};

	protected:

		friend class Domain;

		Device::Name      _name;
		Registry<Domain>  _domains { };

		unsigned          _active_domains { 0 };

		virtual void      _enable()  { };
		virtual void      _disable() { };

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

		void _destroy_domains()
		{
			_domains.for_each([&] (Domain & domain) {
				destroy(domain.md_alloc(), &domain); });
		}

	public:

		/* suspend/resume interface */
		virtual void suspend() { }
		virtual void resume() { }

		/* interface for adding default mappings (used for reserved memory) */
		virtual void add_default_range(Range const &, addr_t) { }

		/* interface for activating default mappings for certain device */
		virtual void enable_default_mappings(Pci::Bdf const &) { }

		/* interface for completing default mappings (enabled IOMMU) */
		virtual void default_mappings_complete() { }

		Device::Name const & name() const { return _name; }

		bool domain_owner(Domain const & domain) const {
			return &domain._io_mmu == this; }

		/* Return true if device requires physical addressing */
		virtual bool mpu() const { return false; }

		/* Create a Io_mmu::Domain object */
		virtual Domain & create_domain(Allocator &,
		                               Ram_allocator &,
		                               Registry<Dma_buffer> const &,
		                               Ram_quota_guard &,
		                               Cap_quota_guard &) = 0;

		virtual void generate(Xml_generator &) { }

		Io_mmu(Io_mmu_devices      & io_mmu_devices,
		       Device::Name  const & name)
		: Io_mmu_devices::Element(io_mmu_devices, *this),
			_name(name)
		{ }

		virtual ~Io_mmu()
		{
			/**
			 * destroying domain objects
			 * any derived class that overrides any virtual method must
			 * call this at the very beginning of its destruction
			 */
			_destroy_domains();
		}
};


class Driver::Io_mmu_factory : private Genode::Registry<Io_mmu_factory>::Element
{
	protected:

		Device::Type  _type;

	public:

		Io_mmu_factory(Registry<Io_mmu_factory> & registry,
		              Device::Type      const & type)
		: Registry<Io_mmu_factory>::Element(registry, *this),
		  _type(type)
		{ }

		virtual ~Io_mmu_factory() { }

		bool matches(Device const & dev) {
			return dev.type() == _type; }

		virtual void create(Allocator &,
		                    Io_mmu_devices &,
		                    Device const &) = 0;
};


#endif /* _SRC__DRIVERS__PLATFORM__IO_MMU_H_ */
