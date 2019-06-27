/*
 * \brief  Representation of a route to a service
 * \author Norman Feske
 * \date   2019-02-25
 */

/*
 * Copyright (C) 2019 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _MODEL__ROUTE_H_
#define _MODEL__ROUTE_H_

#include <types.h>
#include <model/service.h>

namespace Sculpt { struct Route; }


struct Sculpt::Route : List_model<Route>::Element
{
	typedef String<32> Id;
	typedef String<80> Info;

	static char const *xml_type(Service::Type type)
	{
		switch (type) {
		case Service::Type::AUDIO_IN:    return "audio_in";
		case Service::Type::AUDIO_OUT:   return "audio_out";
		case Service::Type::BLOCK:       return "block";
		case Service::Type::FILE_SYSTEM: return "file_system";
		case Service::Type::NIC:         return "nic";
		case Service::Type::NITPICKER:   return "nitpicker";
		case Service::Type::RM:          return "rm";
		case Service::Type::IO_MEM:      return "io_mem";
		case Service::Type::IO_PORT:     return "io_port";
		case Service::Type::IRQ:         return "irq";
		case Service::Type::REPORT:      return "report";
		case Service::Type::ROM:         return "rom";
		case Service::Type::TERMINAL:    return "terminal";
		case Service::Type::TRACE:       return "trace";
		case Service::Type::USB:         return "usb";
		case Service::Type::RTC:         return "rtc";
		case Service::Type::PLATFORM:    return "platform";
		case Service::Type::VM:          return "vm";
		case Service::Type::UNDEFINED:   break;
		}
		return "undefined";
	}

	static char const *_pretty_name(Service::Type type)
	{
		switch (type) {
		case Service::Type::AUDIO_IN:    return "Audio input";
		case Service::Type::AUDIO_OUT:   return "Audio output";
		case Service::Type::BLOCK:       return "Block device";
		case Service::Type::FILE_SYSTEM: return "File system";
		case Service::Type::NIC:         return "Network";
		case Service::Type::NITPICKER:   return "GUI";
		case Service::Type::RM:          return "Region maps";
		case Service::Type::IO_MEM:      return "Direct memory-mapped I/O";
		case Service::Type::IO_PORT:     return "Direct port I/O";
		case Service::Type::IRQ:         return "Direct device interrupts";
		case Service::Type::REPORT:      return "Report";
		case Service::Type::ROM:         return "ROM";
		case Service::Type::TERMINAL:    return "Terminal";
		case Service::Type::TRACE:       return "Tracing";
		case Service::Type::USB:         return "USB";
		case Service::Type::RTC:         return "Real-time clock";
		case Service::Type::PLATFORM:    return "Device access";
		case Service::Type::VM:          return "Hardware-based virtualization";
		case Service::Type::UNDEFINED:   break;
		}
		return "<undefined>";
	}

	static Service::Type _required(Xml_node node)
	{
		for (unsigned i = 0; i < (unsigned)Service::Type::UNDEFINED; i++) {
			Service::Type const s = (Service::Type)i;
			if (node.has_type(xml_type(s)))
				return s;
		}

		return Service::Type::UNDEFINED;
	}

	Service::Type const required;
	Label         const required_label;

	Constructible<Service> selected_service { };

	Id selected_service_id { };

	/**
	 * Constructor
	 *
	 * \param required  sub node of a runtime's <requires> node
	 */
	Route(Xml_node node)
	:
		required(_required(node)),
		required_label(node.attribute_value("label", Label()))
	{ }

	void print(Output &out) const
	{
		Genode::print(out, _pretty_name(required));
		if (required_label.valid())
			Genode::print(out, " (", Pretty(required_label), ") ");
	}

	void gen_xml(Xml_generator &xml) const
	{
		if (!selected_service.constructed()) {
			warning("no service assigned to route ", *this);
			return;
		}

		gen_named_node(xml, "service", Service::name_attr(required), [&] () {

			if (required_label.valid()) {

				if (selected_service->match_label == Service::Match_label::LAST)
					xml.attribute("label_last", required_label);
				else
					xml.attribute("label", required_label);
			}

			selected_service->gen_xml(xml);
		});
	}

	struct Update_policy
	{
		typedef Route Element;

		Allocator &_alloc;

		Update_policy(Allocator &alloc) : _alloc(alloc) { }

		void destroy_element(Route &elem) { destroy(_alloc, &elem); }

		Route &create_element(Xml_node node)
		{
			return *new (_alloc) Route(node);
		}

		void update_element(Route &, Xml_node) { }

		static bool element_matches_xml_node(Route const &elem, Xml_node node)
		{
			return elem.required == _required(node)
			    && elem.required_label == node.attribute_value("label", Label());
		}

		static bool node_is_element(Xml_node node)
		{
			return _required(node) != Service::Type::UNDEFINED;
		}
	};
};

#endif /* _MODEL__ROUTE_H_ */
