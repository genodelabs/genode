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

#ifndef _SRC__DRIVERS__PLATFORM__DEVICE_H_
#define _SRC__DRIVERS__PLATFORM__DEVICE_H_

#include <base/allocator.h>
#include <base/heap.h>
#include <os/reporter.h>
#include <pci/types.h>
#include <platform_session/device.h>
#include <util/list.h>
#include <util/list_model.h>
#include <util/reconstructible.h>
#include <util/xml_generator.h>

#include <shared_irq.h>
#include <clock.h>
#include <reset.h>
#include <power.h>
#include <device_owner.h>

namespace Driver {

	using namespace Genode;

	class  Device;
	struct Device_reporter;
	struct Device_model;
	struct Device_owner;
}


class Driver::Device : private List_model<Device>::Element
{
	public:

		using Name  = Genode::String<64>;
		using Type  = Genode::String<64>;

		struct Pci_bar
		{
			enum { INVALID = 255 };

			unsigned char number { INVALID };

			bool valid() const { return number < INVALID; }
		};

		struct Owner
		{
			void * obj_id;

			Owner() : obj_id(nullptr) {}
			Owner(Device_owner & owner);

			bool operator == (Owner const & o) const {
				return obj_id == o.obj_id; }

			bool valid() const {
				return obj_id != nullptr; }
		};

		struct Io_mem : List_model<Io_mem>::Element
		{
			using Range = Platform::Device_interface::Range;

			Pci_bar bar;
			Range   range;
			bool    prefetchable;

			Io_mem(Pci_bar bar, Range range, bool pf)
			: bar(bar), range(range), prefetchable(pf) {}

			bool matches(Xml_node const &node) const
			{
				Range r { node.attribute_value<Genode::addr_t>("address", 0),
				          node.attribute_value<Genode::size_t>("size",    0) };
				return (r.start == range.start) && (r.size == range.size);
			}

			static bool type_matches(Xml_node const &node)
			{
				return node.has_type("io_mem");
			}
		};

		struct Irq : List_model<Irq>::Element
		{
			enum Type { LEGACY, MSI, MSIX };

			unsigned              number;
			Type                  type     { LEGACY                          };
			Irq_session::Polarity polarity { Irq_session::POLARITY_UNCHANGED };
			Irq_session::Trigger  mode     { Irq_session::TRIGGER_UNCHANGED  };
			bool                  shared   { false                           };

			Irq(unsigned number) : number(number) {}

			bool matches(Xml_node const &node) const
			{
				return (node.attribute_value<unsigned>("number", 0u) == number);
			}

			static bool type_matches(Xml_node const &node)
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

			bool matches(Xml_node const &node) const
			{
				return (node.attribute_value<uint16_t>("address", 0) == range.addr)
				    && (node.attribute_value<uint16_t>("size",    0) == range.size);
			}

			static bool type_matches(Xml_node const &node)
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

			bool matches(Xml_node const &node) const
			{
				return (node.attribute_value("name",  Name())  == name)
				    && (node.attribute_value("value", Value()) == value);
			}

			static bool type_matches(Xml_node const &node)
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

			bool matches(Xml_node const &node) const
			{
				return (node.attribute_value("name",        Name()) == name)
				    && (node.attribute_value("driver_name", Name()) == driver_name);
			}

			static bool type_matches(Xml_node const &node)
			{
				return node.has_type("clock");
			}
		};

		struct Power_domain : List_model<Power_domain>::Element
		{
			using Name = Genode::String<64>;

			Name name;

			Power_domain(Name name) : name(name) {}

			bool matches(Xml_node const &node) const
			{
				return (node.attribute_value("name", Name()) == name);
			}

			static bool type_matches(Xml_node const &node)
			{
				return node.has_type("power-domain");
			}
		};

		struct Reset_domain : List_model<Reset_domain>::Element
		{
			using Name = Genode::String<64>;

			Name name;

			Reset_domain(Name name) : name(name) {}

			bool matches(Xml_node const &node) const
			{
				return (node.attribute_value("name", Name()) == name);
			}

			static bool type_matches(Xml_node const &node)
			{
				return node.has_type("reset-domain");
			}
		};

		struct Pci_config : List_model<Pci_config>::Element
		{
			addr_t        addr;
			Pci::bus_t    bus_num;
			Pci::dev_t    dev_num;
			Pci::func_t   func_num;
			Pci::vendor_t vendor_id;
			Pci::device_t device_id;
			Pci::class_t  class_code;
			Pci::rev_t    revision;
			Pci::vendor_t sub_vendor_id;
			Pci::device_t sub_device_id;
			bool          bridge;

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
			           bool          bridge)
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
				bridge(bridge)
			{ }

			bool matches(Xml_node const &node) const
			{
				return (node.attribute_value("address", ~0UL) == addr);
			}

			static bool type_matches(Xml_node const &node)
			{
				return node.has_type("pci-config");
			}
		};

		struct Reserved_memory : List_model<Reserved_memory>::Element
		{
			using Range = Platform::Device_interface::Range;

			Range range;

			Reserved_memory(Range range) : range(range) {}

			bool matches(Xml_node const &node) const
			{
				return (node.attribute_value("address", 0UL) == range.start)
				    && (node.attribute_value("size",    0UL) == range.size);
			}

			static bool type_matches(Xml_node const &node)
			{
				return node.has_type("reserved_memory");
			}
		};

		struct Io_mmu : List_model<Io_mmu>::Element
		{
			using Name = Genode::String<64>;

			Name name;

			Io_mmu(Name name) : name(name) {}

			bool matches(Xml_node const &node) const
			{
				return (node.attribute_value("name", Name()) == name);
			}

			static bool type_matches(Xml_node const &node)
			{
				return node.has_type("io_mmu");
			}
		};

		Device(Env & env, Device_model & model, Name name, Type type,
		       bool leave_operational);
		virtual ~Device();

		Name  name()  const;
		Type  type()  const;
		Owner owner() const;

		virtual void acquire(Device_owner &);
		virtual void release(Device_owner &);

		template <typename FN> void for_each_irq(FN const & fn) const
		{
			unsigned idx = 0;
			_irq_list.for_each([&] (Irq const & irq) {
				fn(idx++, irq.number, irq.type, irq.polarity,
				   irq.mode, irq.shared); });
		}

		template <typename FN> void for_each_io_mem(FN const & fn) const
		{
			unsigned idx = 0;
			_io_mem_list.for_each([&] (Io_mem const & iomem) {
				fn(idx++, iomem.range, iomem.bar, iomem.prefetchable); });
		}

		template <typename FN> void for_each_io_port_range(FN const & fn) const
		{
			unsigned idx = 0;
			_io_port_range_list.for_each([&] (Io_port_range const & ipr) {
				fn(idx++, ipr.range, ipr.bar); });
		}

		template <typename FN> void for_pci_config(FN const & fn) const
		{
			/*
			 * we allow only one PCI config per device,
			 * even if more were declared
			 */
			bool found = false;
			_pci_config_list.for_each([&] (Pci_config const & cfg) {
				if (found) {
					warning("Only one pci-config is supported per device!");
					return;
				}
				found = true;
				fn(cfg);
			});
		}

		template <typename FN>
		void for_each_reserved_memory(FN const & fn) const
		{
			unsigned idx = 0;
			_reserved_mem_list.for_each([&] (Reserved_memory const & mem) {
				fn(idx++, mem.range); });
		}

		template <typename FN, typename EMPTY_FN>
		void for_each_io_mmu(FN const & fn, EMPTY_FN const & empty_fn) const
		{
			bool empty = true;
			_io_mmu_list.for_each([&] (Io_mmu const & io_mmu) {
				empty = false;
				fn(io_mmu);
			});

			if (empty)
				empty_fn();
		}

		void generate(Xml_generator &, bool) const;

		void update(Allocator &, Xml_node const &);

		/**
		 * List_model::Element
		 */
		bool matches(Xml_node const &node) const
		{
			return name() == node.attribute_value("name", Device::Name()) &&
			       type() == node.attribute_value("type", Device::Type());
		}

		/**
		 * List_model::Element
		 */
		static bool type_matches(Xml_node const &node)
		{
			return node.has_type("device");
		}

	protected:

		friend class Driver::Device_model;
		friend class List_model<Device>;
		friend class List<Device>;

		Env                       & _env;
		Device_model              & _model;
		Name                  const _name;
		Type                  const _type;
		bool                  const _leave_operational;
		Owner                       _owner {};
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


class Driver::Device_model
{
	private:

		Env                      & _env;
		Heap                     & _heap;
		Device_reporter          & _reporter;
		Device_owner             & _owner;
		List_model<Device>         _model  { };
		Registry<Shared_interrupt> _shared_irqs { };
		Clocks                     _clocks { };
		Resets                     _resets { };
		Powers                     _powers { };

	public:

		void generate(Xml_generator & xml) const;
		void update(Xml_node const & node);
		void device_status_changed();

		Device_model(Env             & env,
		             Heap            & heap,
		             Device_reporter & reporter,
		             Device_owner    & owner)
		: _env(env), _heap(heap), _reporter(reporter), _owner(owner) { }

		~Device_model() { update(Xml_node("<empty/>")); }

		template <typename FN>
		void for_each(FN const & fn) { _model.for_each(fn); }

		template <typename FN>
		void for_each(FN const & fn) const { _model.for_each(fn); }

		template <typename FN>
		void with_shared_irq(unsigned number, FN const & fn)
		{
			_shared_irqs.for_each([&] (Shared_interrupt & sirq) {
				if (sirq.number() == number) fn(sirq); });
		}

		Clocks & clocks() { return _clocks; };
		Resets & resets() { return _resets; };
		Powers & powers() { return _powers; };
};

#endif /* _SRC__DRIVERS__PLATFORM__DEVICE_H_ */
