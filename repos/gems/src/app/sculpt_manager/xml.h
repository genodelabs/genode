/*
 * \brief  Utilities for generating XML
 * \author Norman Feske
 * \date   2018-01-11
 */

/*
 * Copyright (C) 2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _XML_H_
#define _XML_H_

/* Genode includes */
#include <util/xml_generator.h>
#include <base/log.h>

/* local includes */
#include "types.h"

namespace Sculpt {

	template <typename FN>
	static inline void gen_named_node(Xml_generator &xml,
	                                  char const *type, char const *name, FN const &fn)
	{
		xml.node(type, [&] () {
			xml.attribute("name", name);
			fn();
		});
	}

	static inline void gen_named_node(Xml_generator &xml, char const *type, char const *name)
	{
		xml.node(type, [&] () { xml.attribute("name", name); });
	}

	template <size_t N, typename FN>
	static inline void gen_named_node(Xml_generator &xml,
	                                  char const *type, String<N> const &name, FN const &fn)
	{
		gen_named_node(xml, type, name.string(), fn);
	}

	template <size_t N>
	static inline void gen_named_node(Xml_generator &xml, char const *type, String<N> const &name)
	{
		gen_named_node(xml, type, name.string());
	}

	template <typename SESSION, typename FN>
	static inline void gen_service_node(Xml_generator &xml, FN const &fn)
	{
		gen_named_node(xml, "service", SESSION::service_name(), fn);
	}

	template <typename SESSION>
	static inline void gen_parent_service(Xml_generator &xml)
	{
		gen_named_node(xml, "service", SESSION::service_name());
	};

	template <typename SESSION>
	static inline void gen_parent_route(Xml_generator &xml)
	{
		gen_named_node(xml, "service", SESSION::service_name(), [&] () {
			xml.node("parent", [&] () { }); });
	}

	static inline void gen_parent_rom_route(Xml_generator  &xml,
	                                        Rom_name const &name,
	                                        auto     const &label)
	{
		gen_service_node<Rom_session>(xml, [&] () {
			xml.attribute("label_last", name);
			xml.node("parent", [&] () {
				xml.attribute("label", label); });
		});
	}

	static inline void gen_parent_rom_route(Xml_generator  &xml,
	                                        Rom_name const &name)
	{
		gen_parent_rom_route(xml, name, name);
	}

	template <typename SESSION>
	static inline void gen_provides(Xml_generator &xml)
	{
		xml.node("provides", [&] () {
			gen_named_node(xml, "service", SESSION::service_name()); });
	}

	static inline void gen_common_routes(Xml_generator &xml)
	{
		gen_parent_rom_route(xml, "ld.lib.so");
		gen_parent_route<Cpu_session>    (xml);
		gen_parent_route<Pd_session>     (xml);
		gen_parent_route<Log_session>    (xml);
		gen_parent_route<Timer::Session> (xml);
		gen_parent_route<Report::Session>(xml);
	}

	static inline void gen_common_start_content(Xml_generator   &xml,
	                                            Rom_name  const &name,
	                                            Cap_quota const  caps,
	                                            Ram_quota const  ram,
	                                            Priority  const  priority)
	{
		xml.attribute("name", name);
		xml.attribute("caps", caps.value);
		xml.attribute("priority", (int)priority);
		gen_named_node(xml, "resource", "RAM", [&] () {
			xml.attribute("quantum", String<64>(Number_of_bytes(ram.value))); });
	}

	template <typename T>
	static T _attribute_value(Xml_node node, char const *attr_name)
	{
		return node.attribute_value(attr_name, T{});
	}

	template <typename T, typename... ARGS>
	static T _attribute_value(Xml_node node, char const *sub_node_type, ARGS... args)
	{
		if (!node.has_sub_node(sub_node_type))
			return T{};

		return _attribute_value<T>(node.sub_node(sub_node_type), args...);
	}

	/**
	 * Query attribute value from XML sub node
	 *
	 * The list of arguments except for the last one refer to XML path into the
	 * XML structure. The last argument denotes the queried attribute name.
	 */
	template <typename T, typename... ARGS>
	static T query_attribute(Xml_node node, ARGS &&... args)
	{
		return _attribute_value<T>(node, args...);
	}

	static inline void copy_attributes(Xml_generator &xml, Xml_node const &from)
	{
		using Value = String<64>;
		from.for_each_attribute([&] (Xml_attribute const &attr) {
			Value value { };
			attr.value(value);
			xml.attribute(attr.name().string(), value);
		});
	}

	struct Xml_max_depth { unsigned value; };

	static inline void copy_node(Xml_generator &xml, Xml_node const &from,
	                             Xml_max_depth max_depth = { 5 })
	{
		if (max_depth.value)
			xml.node(from.type().string(), [&] {
				copy_attributes(xml, from);
				from.for_each_sub_node([&] (Xml_node const &sub_node) {
					copy_node(xml, sub_node, { max_depth.value - 1 }); }); });
	}
}

#endif /* _XML_H_ */
