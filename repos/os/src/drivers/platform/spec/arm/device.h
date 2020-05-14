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
	class  Device;
	struct Device_model;
	class  Session_component;
}


class Driver::Device : public Genode::List_model<Device>::Element
{
	public:

		struct Io_mem : Genode::List_model<Io_mem>::Element
		{
			Genode::addr_t              base;
			Genode::size_t              size;
			Genode::Io_mem_connection * io_mem { nullptr };

			Io_mem(Genode::addr_t base, Genode::size_t size)
			: base(base), size(size) {}
		};

		struct Irq : Genode::List_model<Irq>::Element
		{
			unsigned                 number;
			Genode::Irq_connection * irq { nullptr };

			Irq(unsigned number) : number(number) {}
		};

		struct Property : Genode::List_model<Property>::Element
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
		~Device();

		Name name() const;

		bool acquire(Session_component &);
		void release(Session_component &);

		Genode::Irq_session_capability    irq(unsigned idx,
		                                      Session_component & session);
		Genode::Io_mem_session_capability io_mem(unsigned idx,
		                                         Genode::Cache_attribute,
		                                         Session_component & session);

		void report(Genode::Xml_generator &);

	private:

		Genode::size_t _cap_quota_required();
		Genode::size_t _ram_quota_required();

		friend class Driver::Device_model;

		Name                         _name;
		Platform::Session::Label     _session {};
		Genode::List_model<Io_mem>   _io_mem_list {};
		Genode::List_model<Irq>      _irq_list {};
		Genode::List_model<Property> _property_list {};

		/*
		 * Noncopyable
		 */
		Device(Device const &);
		Device &operator = (Device const &);
};


class Driver::Device_model :
	public Genode::List_model<Device>::Update_policy
{
	private:

		Genode::Allocator        & _alloc;
		Genode::List_model<Device> _model {};

	public:

		void update(Genode::Xml_node const & node) {
			_model.update_from_xml(*this, node);
		}

		Device_model(Genode::Allocator      & alloc,
		             Genode::Xml_node const & node)
		: _alloc(alloc) { update(node); }

		~Device_model() {
			_model.destroy_all_elements(*this); }


		template <typename FN>
		void for_each(FN const & fn) { _model.for_each(fn); }


		/***********************
		 ** Update_policy API **
		 ***********************/

		void        destroy_element(Device & device);
		Device &    create_element(Genode::Xml_node node);
		void        update_element(Device & device, Genode::Xml_node node);
		static bool element_matches_xml_node(Device const &,
		                                     Genode::Xml_node);
		static bool node_is_element(Genode::Xml_node);
};

#endif /* _SRC__DRIVERS__PLATFORM__SPEC__ARM__DEVICE_H_ */
