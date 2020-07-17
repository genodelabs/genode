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

#ifndef _SRC__DRIVERS__PLATFORM__SPEC__ARM__DEVICE_H_
#define _SRC__DRIVERS__PLATFORM__SPEC__ARM__DEVICE_H_

#include <base/allocator.h>
#include <io_mem_session/connection.h>
#include <irq_session/connection.h>
#include <platform_session/platform_session.h>
#include <util/list.h>
#include <util/list_model.h>
#include <util/reconstructible.h>
#include <util/xml_generator.h>

namespace Driver {
	using namespace Genode;

	class  Env;
	class  Device;
	struct Device_model;
	class  Session_component;
	struct Irq_update_policy;
	struct Io_mem_update_policy;
	struct Property_update_policy;
}


class Driver::Device : private List_model<Device>::Element
{
	public:

		struct Io_mem : List_model<Io_mem>::Element
		{
			addr_t              base;
			size_t              size;
			Io_mem_connection * io_mem { nullptr };

			Io_mem(addr_t base, size_t size)
			: base(base), size(size) {}
		};

		struct Irq : List_model<Irq>::Element
		{
			unsigned         number;
			Irq_connection * irq { nullptr };

			Irq(unsigned number) : number(number) {}
		};

		struct Property : List_model<Property>::Element
		{
			using Name  = Genode::String<64>;
			using Value = Genode::String<64>;

			Name  name;
			Value value;

			Property(Name name, Value value)
			: name(name), value(value) {}
		};

		using Name = Genode::String<64>;

		Device(Name name);
		virtual ~Device();

		Name name() const;

		virtual bool acquire(Session_component &);
		virtual void release(Session_component &);

		Irq_session_capability    irq(unsigned idx,
		                              Session_component & session);
		Io_mem_session_capability io_mem(unsigned idx, Cache_attribute,
		                                 Session_component & session);

		void report(Xml_generator &, Session_component &);

	protected:

		virtual void _report_platform_specifics(Xml_generator &,
		                                        Session_component &) {}

		size_t _cap_quota_required();
		size_t _ram_quota_required();

		friend class Driver::Device_model;
		friend class List_model<Device>;
		friend class List<Device>;

		Name                     _name;
		Platform::Session::Label _session {};
		List_model<Io_mem>       _io_mem_list {};
		List_model<Irq>          _irq_list {};
		List_model<Property>     _property_list {};

		/*
		 * Noncopyable
		 */
		Device(Device const &);
		Device &operator = (Device const &);
};


class Driver::Device_model :
	public List_model<Device>::Update_policy
{
	private:

		Driver::Env      & _env;
		List_model<Device> _model {};

	public:

		void update(Xml_node const & node) {
			_model.update_from_xml(*this, node);
		}

		Device_model(Driver::Env & env)
		: _env(env) { }

		~Device_model() {
			_model.destroy_all_elements(*this); }

		template <typename FN>
		void for_each(FN const & fn) { _model.for_each(fn); }


		/***********************
		 ** Update_policy API **
		 ***********************/

		void        destroy_element(Device & device);
		Device &    create_element(Xml_node node);
		void        update_element(Device & device, Xml_node node);
		static bool element_matches_xml_node(Device const & dev,
		                                     Genode::Xml_node n) {
			return dev.name() == n.attribute_value("name", Device::Name()); }

		static bool node_is_element(Genode::Xml_node node) {
			return node.has_type("device"); }
};


struct Driver::Irq_update_policy : Genode::List_model<Device::Irq>::Update_policy
{
	Genode::Allocator & alloc;

	Irq_update_policy(Genode::Allocator & alloc) : alloc(alloc) {}

	void destroy_element(Element & irq) {
		Genode::destroy(alloc, &irq); }

	Element & create_element(Genode::Xml_node node)
	{
		unsigned number = node.attribute_value<unsigned>("number", 0);
		return *(new (alloc) Element(number));
	}

	void update_element(Element &, Genode::Xml_node) {}

	static bool element_matches_xml_node(Element const & irq, Genode::Xml_node node)
	{
		unsigned number = node.attribute_value<unsigned>("number", 0);
		return number == irq.number;
	}

	static bool node_is_element(Genode::Xml_node node)
	{
		return node.has_type("irq");
	}
};


struct Driver::Io_mem_update_policy : Genode::List_model<Device::Io_mem>::Update_policy
{
	Genode::Allocator & alloc;

	Io_mem_update_policy(Genode::Allocator & alloc) : alloc(alloc) {}

	void destroy_element(Element & iomem) {
		Genode::destroy(alloc, &iomem); }

	Element & create_element(Genode::Xml_node node)
	{
		Genode::addr_t base = node.attribute_value<Genode::addr_t>("address", 0);
		Genode::size_t size = node.attribute_value<Genode::size_t>("size",    0);
		return *(new (alloc) Element(base, size));
	}

	void update_element(Element &, Genode::Xml_node) {}

	static bool element_matches_xml_node(Element const & iomem, Genode::Xml_node node)
	{
		Genode::addr_t base = node.attribute_value<Genode::addr_t>("address", 0);
		Genode::size_t size = node.attribute_value<Genode::size_t>("size",    0);
		return (base == iomem.base) && (size == iomem.size);
	}

	static bool node_is_element(Genode::Xml_node node)
	{
		return node.has_type("io_mem");
	}
};


struct Driver::Property_update_policy : Genode::List_model<Device::Property>::Update_policy
{
	Genode::Allocator & alloc;

	Property_update_policy(Genode::Allocator & alloc) : alloc(alloc) {}

	void destroy_element(Element & p) {
		Genode::destroy(alloc, &p); }

	Element & create_element(Genode::Xml_node node)
	{
		return *(new (alloc)
			Element(node.attribute_value("name",  Element::Name()),
			        node.attribute_value("value", Element::Value())));
	}

	void update_element(Element &, Genode::Xml_node) {}

	static bool element_matches_xml_node(Element const & prop, Genode::Xml_node node)
	{
		Element::Name  n = node.attribute_value("name", Element::Name());
		Element::Value v = node.attribute_value("value", Element::Value());
		return (n == prop.name) && (v == prop.value);
	}

	static bool node_is_element(Genode::Xml_node node) {
		return node.has_type("property"); }
};

#endif /* _SRC__DRIVERS__PLATFORM__SPEC__ARM__DEVICE_H_ */
