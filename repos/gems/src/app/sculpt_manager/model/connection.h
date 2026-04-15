/*
 * \brief  Representation of a connection from client to service
 * \author Norman Feske
 * \date   2019-02-25
 */

/*
 * Copyright (C) 2019 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _MODEL__CONNECTION_H_
#define _MODEL__CONNECTION_H_

#include <xml.h>
#include <types.h>
#include <model/service.h>

namespace Sculpt { struct Connection; }


struct Sculpt::Connection : List_model<Connection>::Element
{
	using Id   = String<32>;
	using Info = String<80>;
	using Name = Service::Name;

	static char const *_pretty_name(Service::Type type)
	{
		switch (type) {
		case Service::Type::AUDIO_IN:    return "Audio input";
		case Service::Type::AUDIO_OUT:   return "Audio output";
		case Service::Type::BLOCK:       return "Block device";
		case Service::Type::EVENT:       return "Event";
		case Service::Type::CAPTURE:     return "Capture";
		case Service::Type::FS:          return "File system";
		case Service::Type::NIC:         return "Network";
		case Service::Type::UPLINK:      return "Network uplink";
		case Service::Type::GUI:         return "GUI";
		case Service::Type::GPU:         return "GPU";
		case Service::Type::RM:          return "Region maps";
		case Service::Type::I2C:         return "I2c";
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

	static Service::Type _required(Node const &node)
	{
		String<16> type = node.type();
		if (type == "file_system") type = "fs";

		for (unsigned i = 0; i < (unsigned)Service::Type::UNDEFINED; i++) {
			Service::Type const s = (Service::Type)i;
			if (type == Service::node_type(s))
				return s;
		}

		return Service::Type::UNDEFINED;
	}

	Service::Type const required;
	Name          const required_name;

	Constructible<Service> selected_service { };

	Id selected_service_id { };

	/*
	 * Directory selection of a file-system connection
	 */

	using Path = String<256>;

	struct Browsed
	{
		Id   service_id;
		Path path;

		static constexpr unsigned MAX_LEVELS = 10;
		unsigned _dirent_index[MAX_LEVELS];

		void index_at_level(unsigned level, unsigned index)
		{
			if (level < MAX_LEVELS)
				_dirent_index[level] = index;
		}

		unsigned index_at_level(unsigned level) const
		{
			return level < MAX_LEVELS ? _dirent_index[level] : 0;
		}

	} browsed { };

	Path selected_path { };

	void deselect()
	{
		selected_service.destruct();
		selected_service_id = { };
		selected_path = { };
	}

	/**
	 * Constructor
	 *
	 * \param required  sub node of a runtime's <requires> node
	 */
	Connection(Node const &node)
	:
		required(_required(node)),
		required_name(node.attribute_value("name",
		                 node.attribute_value("label", Name()))) /* deprecated */
	{ }

	void print(Output &out) const
	{
		Genode::print(out, _pretty_name(required));
		if (required_name.valid())
			Genode::print(out, " (", Pretty(required_name), ") ");
	}

	void generate(Generator &g) const
	{
		if (!selected_service.constructed()) {
			warning("no service assigned to connection ", *this);
			return;
		}

		g.node(Service::node_type(required), [&] {

			if (required_name.valid())
				g.attribute("name", required_name);

			if (selected_service->type == Service::Type::FS)
				selected_service->generate(g, [&] {
					if (selected_path.length() > 1)
						g.attribute("prepend_resource", selected_path); });
			else
				selected_service->generate(g);
		});
	}

	bool matches(Node const &node) const
	{
		return required == _required(node)
		    && required_name == node.attribute_value("name",
		                           node.attribute_value("label", Name())); /* deprecated */
	}

	static bool type_matches(Node const &node)
	{
		return _required(node) != Service::Type::UNDEFINED;
	}
};

#endif /* _MODEL__CONNECTION_H_ */
