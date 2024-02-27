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

#include <xml.h>
#include <types.h>
#include <model/service.h>

namespace Sculpt { struct Route; }


struct Sculpt::Route : List_model<Route>::Element
{
	using Id    = String<32>;
	using Info  = String<80>;
	using Label = Service::Label;

	static char const *xml_type(Service::Type type)
	{
		switch (type) {
		case Service::Type::AUDIO_IN:    return "audio_in";
		case Service::Type::AUDIO_OUT:   return "audio_out";
		case Service::Type::BLOCK:       return "block";
		case Service::Type::EVENT:       return "event";
		case Service::Type::CAPTURE:     return "capture";
		case Service::Type::FILE_SYSTEM: return "file_system";
		case Service::Type::NIC:         return "nic";
		case Service::Type::UPLINK:      return "uplink";
		case Service::Type::GUI:         return "gui";
		case Service::Type::GPU:         return "gpu";
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
		case Service::Type::PIN_STATE:   return "pin_state";
		case Service::Type::PIN_CONTROL: return "pin_control";
		case Service::Type::VM:          return "vm";
		case Service::Type::PD:          return "pd";
		case Service::Type::PLAY:        return "play";
		case Service::Type::RECORD:      return "record";
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
		case Service::Type::EVENT:       return "Event";
		case Service::Type::CAPTURE:     return "Capture";
		case Service::Type::FILE_SYSTEM: return "File system";
		case Service::Type::NIC:         return "Network";
		case Service::Type::UPLINK:      return "Network uplink";
		case Service::Type::GUI:         return "GUI";
		case Service::Type::GPU:         return "GPU";
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
		case Service::Type::PIN_STATE:   return "GPIO pin state";
		case Service::Type::PIN_CONTROL: return "GPIO pin control";
		case Service::Type::VM:          return "Hardware-based virtualization";
		case Service::Type::PD:          return "Protection domain";
		case Service::Type::PLAY:        return "Play";
		case Service::Type::RECORD:      return "Record";
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

		gen_named_node(xml, "service", Service::name_attr(required), [&] {

			if (required_label.valid()) {

				if (selected_service->match_label == Service::Match_label::LAST)
					xml.attribute("label_last", required_label);
				else
					xml.attribute("label", required_label);
			}

			selected_service->gen_xml(xml);
		});
	}

	bool matches(Xml_node node) const
	{
		return required == _required(node)
		    && required_label == node.attribute_value("label", Label());
	}

	static bool type_matches(Xml_node node)
	{
		return _required(node) != Service::Type::UNDEFINED;
	}
};

#endif /* _MODEL__ROUTE_H_ */
