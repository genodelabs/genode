/*
 * \brief  Platform driver - Device abstraction
 * \author Stefan Kalkowski
 * \date   2020-04-30
 */

/*
 * Copyright (C) 2020 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _SRC__DRIVER__PLATFORM__DEVICE_H_
#define _SRC__DRIVER__PLATFORM__DEVICE_H_

#include <base/allocator.h>
#include <base/heap.h>
#include <os/reporter.h>
#include <pci/types.h>
#include <platform_session/device.h>
#include <util/list.h>
#include <util/list_model.h>

#include <clock.h>
#include <device_owner.h>
#include <io_mmu.h>
#include <irq_controller.h>
#include <kernel_io_mmu.h>
#include <power.h>
#include <reserved_memory_handler.h>
#include <reset.h>
#include <shared_irq.h>

namespace Driver {

	using namespace Genode;

	class  Main;
	class  Device;
	struct Device_reporter;
	struct Device_model;
	struct Device_owner;
	struct Reserved_memory_handler;
}


class Driver::Device : private List_model<Device>::Element
{
	public:

		using Name  = Device_name;
		using Type  = Device_type;

		struct Pci_bar
		{
			enum { INVALID = 255 };

			unsigned char number { INVALID };

			bool valid() const { return number < INVALID; }
		};

		struct Io_mem : List_model<Io_mem>::Element
		{
			using Range = Platform::Device_interface::Range;

			Pci_bar bar;
			Range   range;
			bool    write_combined;

			Io_mem(Pci_bar bar, Range range, bool wc)
			: bar(bar), range(range), write_combined(wc) {}

			bool matches(Node const &node) const
			{
				Range r { node.attribute_value<Genode::addr_t>("address", 0),
				          node.attribute_value<Genode::size_t>("size",    0) };
				return (r.start == range.start) && (r.size == range.size);
			}

			static bool type_matches(Node const &node)
			{
				return node.has_type("io_mem");
			}
		};

		struct Irq : List_model<Irq>::Element
		{
			unsigned              number;
			Irq_session::Type     type     { Irq_session::TYPE_LEGACY        };
			Irq_session::Polarity polarity { Irq_session::POLARITY_UNCHANGED };
			Irq_session::Trigger  mode     { Irq_session::TRIGGER_UNCHANGED  };
			bool                  shared   { false                           };

			Irq(unsigned number) : number(number) {}

			bool matches(Node const &node) const
			{
				return (node.attribute_value<unsigned>("number", 0u) == number);
			}

			static bool type_matches(Node const &node)
			{
				return node.has_type("irq");
			}
		};

		struct Io_port_range : List_model<Io_port_range>::Element
		{
			struct Range { uint16_t addr; uint16_t size; };

			Pci_bar  bar;
			Range    range;

			Io_port_range(Pci_bar bar, Range range)
			: bar(bar), range(range) {}

			bool matches(Node const &node) const
			{
				return (node.attribute_value<uint16_t>("address", 0) == range.addr)
				    && (node.attribute_value<uint16_t>("size",    0) == range.size);
			}

			static bool type_matches(Node const &node)
			{
				return node.has_type("io_port_range");
			}
		};

		struct Property : List_model<Property>::Element
		{
			using Name  = Genode::String<64>;
			using Value = Genode::String<64>;

			Name  name;
			Value value;

			Property(Name name, Value value)
			: name(name), value(value) {}

			bool matches(Node const &node) const
			{
				return (node.attribute_value("name",  Name())  == name)
				    && (node.attribute_value("value", Value()) == value);
			}

			static bool type_matches(Node const &node)
			{
				return node.has_type("property");
			}
		};

		struct Clock : List_model<Clock>::Element
		{
			using Name  = Genode::String<64>;

			Name          name;
			Name          parent;
			Name          driver_name;
			unsigned long rate;

			Clock(Name name,
			      Name parent,
			      Name driver_name,
			      unsigned long rate)
			: name(name), parent(parent),
			  driver_name(driver_name), rate(rate) {}

			bool matches(Node const &node) const
			{
				return (node.attribute_value("name",        Name()) == name)
				    && (node.attribute_value("driver_name", Name()) == driver_name);
			}

			static bool type_matches(Node const &node)
			{
				return node.has_type("clock");
			}
		};

		struct Power_domain : List_model<Power_domain>::Element
		{
			using Name = Genode::String<64>;

			Name name;

			Power_domain(Name name) : name(name) {}

			bool matches(Node const &node) const
			{
				return (node.attribute_value("name", Name()) == name);
			}

			static bool type_matches(Node const &node)
			{
				return node.has_type("power-domain");
			}
		};

		struct Reset_domain : List_model<Reset_domain>::Element
		{
			using Name = Genode::String<64>;

			Name name;

			Reset_domain(Name name) : name(name) {}

			bool matches(Node const &node) const
			{
				return (node.attribute_value("name", Name()) == name);
			}

			static bool type_matches(Node const &node)
			{
				return node.has_type("reset-domain");
			}
		};

		struct Pci_config : List_model<Pci_config>::Element
		{
			addr_t        const addr;
			Pci::bus_t    const bus_num;
			Pci::dev_t    const dev_num;
			Pci::func_t   const func_num;
			Pci::vendor_t const vendor_id;
			Pci::device_t const device_id;
			Pci::class_t  const class_code;
			Pci::rev_t    const revision;
			Pci::vendor_t const sub_vendor_id;
			Pci::device_t const sub_device_id;

			/* bridge specific (header type 0x1) values */
			bool     const bridge;
			uint16_t const io_base_limit;
			uint16_t const memory_base;
			uint16_t const memory_limit;
			uint16_t const bridge_control;
			unsigned const prefetch_memory_base;
			unsigned const prefetch_memory_base_upper;
			unsigned const prefetch_memory_limit_upper;
			unsigned const io_base_limit_upper;
			unsigned const expansion_rom_base;

			Pci_config(addr_t        addr,
			           Pci::bus_t    bus_num,
			           Pci::dev_t    dev_num,
			           Pci::func_t   func_num,
			           Pci::vendor_t vendor_id,
			           Pci::device_t device_id,
			           Pci::class_t  class_code,
			           Pci::rev_t    revision,
			           Pci::vendor_t sub_vendor_id,
			           Pci::device_t sub_device_id,
			           bool          bridge,
			           uint16_t      io_base_limit,
			           uint16_t      memory_base,
			           uint16_t      memory_limit,
			           unsigned      prefetch_memory_base,
			           unsigned      prefetch_memory_base_upper,
			           unsigned      prefetch_memory_limit_upper,
			           unsigned      io_base_limit_upper,
			           unsigned      expansion_rom_base,
			           uint16_t      bridge_control)
			:
				addr(addr),
				bus_num(bus_num),
				dev_num(dev_num),
				func_num(func_num),
				vendor_id(vendor_id),
				device_id(device_id),
				class_code(class_code),
				revision(revision),
				sub_vendor_id(sub_vendor_id),
				sub_device_id(sub_device_id),
				bridge(bridge),
				io_base_limit(io_base_limit),
				memory_base(memory_base),
				memory_limit(memory_limit),
				bridge_control(bridge_control),
				prefetch_memory_base(prefetch_memory_base),
				prefetch_memory_base_upper(prefetch_memory_base_upper),
				prefetch_memory_limit_upper(prefetch_memory_limit_upper),
				io_base_limit_upper(io_base_limit_upper),
				expansion_rom_base(expansion_rom_base)
			{ }

			bool matches(Node const &node) const
			{
				return (node.attribute_value("address", ~0UL) == addr);
			}

			static bool type_matches(Node const &node)
			{
				return node.has_type("pci-config");
			}
		};

		struct Reserved_memory : List_model<Reserved_memory>::Element
		{
			using Range = Platform::Device_interface::Range;

			Range range;

			Reserved_memory(Range range) : range(range) {}

			bool matches(Node const &node) const
			{
				return (node.attribute_value("address", 0UL) == range.start)
				    && (node.attribute_value("size",    0UL) == range.size);
			}

			static bool type_matches(Node const &node)
			{
				return node.has_type("reserved_memory");
			}
		};

		struct Io_mmu : List_model<Io_mmu>::Element
		{
			using Name = Genode::String<64>;

			Name name;

			Io_mmu(Name name) : name(name) {}

			bool matches(Node const &node) const
			{
				return (node.attribute_value("name", Name()) == name);
			}

			static bool type_matches(Node const &node)
			{
				return node.has_type("io_mmu");
			}
		};

		Device(Env &env, Device_model &model, Name name, Type type,
		       bool leave_operational);
		virtual ~Device();

		Name name() const;
		Type type() const;

		bool owned() const;
		bool owner(Device_owner&) const;

		virtual void acquire(Device_owner &);
		virtual void release(Device_owner &);

		void for_each_irq(auto const &fn) const
		{
			unsigned idx = 0;
			_irq_list.for_each([&] (Irq const &irq) {
				fn(idx++, irq.number, irq.type, irq.polarity,
				   irq.mode, irq.shared); });
		}

		void for_each_io_mem(auto const &fn) const
		{
			unsigned idx = 0;
			_io_mem_list.for_each([&] (Io_mem const &iomem) {
				fn(idx++, iomem.range, iomem.bar, iomem.write_combined); });
		}

		void for_each_io_port_range(auto const &fn) const
		{
			unsigned idx = 0;
			_io_port_range_list.for_each([&] (Io_port_range const &ipr) {
				fn(idx++, ipr.range, ipr.bar); });
		}

		void for_each_property(auto const &fn) const
		{
			_property_list.for_each([&] (Property const &p) {
				fn(p.name, p.value); });
		}

		void for_pci_config(auto const &fn) const
		{
			/*
			 * we allow only one PCI config per device,
			 * even if more were declared
			 */
			bool found = false;
			_pci_config_list.for_each([&] (Pci_config const &cfg) {
				if (found) {
					warning("Only one pci-config is supported per device!");
					return;
				}
				found = true;
				fn(cfg);
			});
		}

		void for_each_reserved_memory(auto const &fn) const
		{
			unsigned idx = 0;
			_reserved_mem_list.for_each([&] (Reserved_memory const &mem) {
				fn(idx++, mem.range); });
		}

		void for_each_io_mmu(auto const &fn, auto const &empty_fn) const
		{
			bool empty = true;
			_io_mmu_list.for_each([&] (Io_mmu const &io_mmu) {
				empty = false;
				fn(io_mmu);
			});

			if (empty)
				empty_fn();
		}

		void with_io_mmu(Io_mmu::Name const &name, auto const &fn) const
		{
			_io_mmu_list.for_each([&] (auto const &io_mmu) {
				if (io_mmu.name == name) fn(io_mmu); });
		}

		void generate(Generator &, bool) const;

		void update(Allocator &, Node const &, Reserved_memory_handler &);

		/**
		 * List_model::Element
		 */
		bool matches(Node const &node) const
		{
			return name() == node.attribute_value("name", Device::Name()) &&
			       type() == node.attribute_value("type", Device::Type());
		}

		/**
		 * List_model::Element
		 */
		static bool type_matches(Node const &node)
		{
			return node.has_type("device");
		}

	protected:

		friend class Driver::Device_model;
		friend class List_model<Device>;
		friend class List<Device>;

		struct Owner_id
		{
			void * obj_id;

			Owner_id() : obj_id(nullptr) {}
			Owner_id(Device_owner &owner)
			: obj_id((void*)&owner) {}

			bool valid() const {
				return obj_id != nullptr; }
		};

		Env                        &_env;
		Device_model               &_model;
		Name                  const _name;
		Type                  const _type;
		bool                  const _leave_operational;
		Owner_id                    _owner {};
		List_model<Io_mem>          _io_mem_list {};
		List_model<Irq>             _irq_list {};
		List_model<Io_port_range>   _io_port_range_list {};
		List_model<Property>        _property_list {};
		List_model<Clock>           _clock_list {};
		List_model<Power_domain>    _power_domain_list {};
		List_model<Reset_domain>    _reset_domain_list {};
		List_model<Pci_config>      _pci_config_list {};
		List_model<Reserved_memory> _reserved_mem_list {};
		List_model<Io_mmu>          _io_mmu_list {};

		/*
		 * Noncopyable
		 */
		Device(Device const &);
		Device &operator = (Device const &);
};


struct Driver::Device_reporter
{
	virtual ~Device_reporter() {}

	virtual void update_report() = 0;
};


class Driver::Device_model : public Device_owner
{
	private:

		Env                       &_env;
		Heap                      &_heap;
		Device_reporter           &_reporter;
		List_model<Device>         _model  { };
		Registry<Shared_interrupt> _shared_irqs { };
		Clocks                     _clocks { };
		Resets                     _resets { };
		Powers                     _powers { };

		Io_mmu_devices           _io_mmus { };
		Registry<Io_mmu_factory> _io_mmu_factories { };

		Registry<Irq_controller>         _irq_controllers { };
		Registry<Irq_controller_factory> _irq_controller_factories { };

		bool const _kernel_controls_io_mmu {};
		Constructible<Kernel_io_mmu> _kernel_io_mmu {};

		void _acquire_io_mmus();
		void _acquire_irq_controller();
		void _detect_shared_interrupts();

		friend class Main;

	public:

		void report_devices(Generator &) const;
		void report_iommus(Generator &) const;
		void update(Node const &node, Reserved_memory_handler &);
		void device_status_changed();

		Device_model(Env             &env,
		             Heap            &heap,
		             Device_reporter &reporter,
		             bool             kernel_io_mmu)
		:
			_env(env), _heap(heap), _reporter(reporter),
			_kernel_controls_io_mmu(kernel_io_mmu)
		{ }

		~Device_model()
		{
			Reserved_memory_handler dummy { };
			update(Node(), dummy);
		}

		void for_each(auto const &fn) { _model.for_each(fn); }

		void for_each(auto const &fn) const { _model.for_each(fn); }

		void for_each_io_mmu(auto const &fn) {
			_io_mmus.for_each([&] (auto &io_mmu) { fn(io_mmu); }); }

		void for_each_io_mmu(auto const &fn) const {
			_io_mmus.for_each([&] (auto const &io_mmu) { fn(io_mmu); }); }

		void for_each_irq_controller(auto const &fn) {
			_irq_controllers.for_each([&] (auto &ic) { fn(ic); }); }

		void with_shared_irq(unsigned number, auto const &fn)
		{
			_shared_irqs.for_each([&] (auto &sirq) {
				if (sirq.number() == number) fn(sirq); });
		}

		void with_io_mmu(Device::Name const &name, auto const &fn)
		{
			for_each_io_mmu([&] (auto &io_mmu) {
				if (_kernel_io_mmu.constructed() ||
				    io_mmu.name() == name) fn(io_mmu); });
		}

		void with_io_mmu(Device const &dev, auto const &fn)
		{
			bool found = false;
			dev.for_each_io_mmu([&] (auto const &im) {
				if (found)
					return;
				found = true;
				with_io_mmu(im.name, fn);
			}, [] () { /* ignore */ });
		}

		void with_irq_controller(Device::Name const &name, auto const &fn)
		{
			for_each_irq_controller([&] (auto &ic) {
				if (ic.name() == name) fn(ic); });
		}

		Clocks & clocks() { return _clocks; };
		Resets & resets() { return _resets; };
		Powers & powers() { return _powers; };

		void finalize_devices_preparation();


		/******************
		 ** Device_owner **
		 ******************/

		void enable_device(Device const &) override { };
		void disable_device(Device const &device) override;
};

#endif /* _SRC__DRIVER__PLATFORM__DEVICE_H_ */
