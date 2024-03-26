/*
 * \brief  Representation of service that can be targeted by a route
 * \author Norman Feske
 * \date   2019-02-25
 */

/*
 * Copyright (C) 2019 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _MODEL__SERVICE_H_
#define _MODEL__SERVICE_H_

#include <string.h>

namespace Sculpt { struct Service; }


struct Sculpt::Service
{
	using Type_name = String<16>;
	using Info      = String<32>;
	using Label     = String<64>;

	enum class Type {
		AUDIO_IN, AUDIO_OUT, BLOCK, EVENT, CAPTURE, FILE_SYSTEM, NIC, GUI, GPU,
		RM, IO_MEM, IO_PORT, IRQ, REPORT, ROM, TERMINAL, TRACE, USB, RTC,
		PLATFORM, PIN_STATE, PIN_CONTROL, VM, PD, UPLINK, UNDEFINED };

	enum class Match_label { EXACT, LAST };

	Start_name  server { }; /* invalid for parent service */
	Type        type;
	Label       label;
	Info        info;
	Match_label match_label { Match_label::EXACT };

	/**
	 * Return name attribute value of <service name="..."> node
	 */
	static char const *name_attr(Type type)
	{
		switch (type) {
		case Type::AUDIO_IN:    return "Audio_in";
		case Type::AUDIO_OUT:   return "Audio_out";
		case Type::BLOCK:       return "Block";
		case Type::EVENT:       return "Event";
		case Type::CAPTURE:     return "Capture";
		case Type::FILE_SYSTEM: return "File_system";
		case Type::NIC:         return "Nic";
		case Type::UPLINK:      return "Uplink";
		case Type::GUI:         return "Gui";
		case Type::GPU:         return "Gpu";
		case Type::RM:          return "RM";
		case Type::IO_MEM:      return "IO_MEM";
		case Type::IO_PORT:     return "IO_PORT";
		case Type::IRQ:         return "IRQ";
		case Type::REPORT:      return "Report";
		case Type::ROM:         return "ROM";
		case Type::TERMINAL:    return "Terminal";
		case Type::TRACE:       return "TRACE";
		case Type::USB:         return "Usb";
		case Type::RTC:         return "Rtc";
		case Type::PLATFORM:    return "Platform";
		case Type::PIN_STATE:   return "Pin_state";
		case Type::PIN_CONTROL: return "Pin_control";
		case Type::VM:          return "VM";
		case Type::PD:          return "PD";
		case Type::UNDEFINED:   break;
		}
		return "undefined";
	}

	/**
	 * Constructor for child service
	 */
	Service(Start_name const &server, Type type, Label const &label)
	: server(server), type(type), label(label), info(Subst("_", " ", server)) { }

	/**
	 * Constructor for default_fs_rw
	 */
	Service(Start_name const &server, Type type, Label const &label, Info const &info)
	: server(server), type(type), label(label), info(info) { }

	/**
	 * Constructor for parent service
	 */
	Service(Type type, Info const &info, Label const &label = Label(),
	        Match_label match_label = Match_label::EXACT)
	:
		type(type), label(label), info(info), match_label(match_label)
	{ }

	void gen_xml(Xml_generator &xml) const
	{
		bool const parent = !server.valid();

		xml.node(parent ? "parent" : "child", [&] {

			if (!parent)
				xml.attribute("name", server);

			if (label.valid() && match_label == Match_label::EXACT)
				xml.attribute("label", label);
		});
	}
};

#endif /* _MODEL__SERVICE_H_ */
