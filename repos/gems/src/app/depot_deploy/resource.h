/*
 * \brief  Resource types
 * \author Norman Feske
 * \date   2026-03-10
 */

/*
 * Copyright (C) 2026 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _RESOURCE_H_
#define _RESOURCE_H_

/* Genode includes */
#include <util/dictionary.h>
#include <base/node.h>

namespace Depot_deploy {
	using namespace Genode;
	struct Resource;
}


struct Depot_deploy::Resource
{
	enum Type {
		AUDIO_IN, AUDIO_OUT, BLOCK, EVENT, CAPTURE, FS, NIC, GUI, GPU, LOG,
		RM, IO_MEM, IO_PORT, IRQ, REPORT, ROM, TERMINAL, TRACE, USB, RTC,
		I2C, PLATFORM, PIN_STATE, PIN_CONTROL, VM, PD, UPLINK, PLAY, RECORD,
		UNDEFINED
	};

	using Node_type = String<16>;  /* runtime node type */
	using Name      = String<32>;

	Type type;

	char const *service_name() const
	{
		switch (type) {
		case AUDIO_IN:    return "Audio_in";
		case AUDIO_OUT:   return "Audio_out";
		case BLOCK:       return "Block";
		case EVENT:       return "Event";
		case CAPTURE:     return "Capture";
		case FS:          return "File_system";
		case NIC:         return "Nic";
		case UPLINK:      return "Uplink";
		case GUI:         return "Gui";
		case GPU:         return "Gpu";
		case LOG:         return "LOG";
		case RM:          return "RM";
		case I2C:         return "I2c";
		case IO_MEM:      return "IO_MEM";
		case IO_PORT:     return "IO_PORT";
		case IRQ:         return "IRQ";
		case REPORT:      return "Report";
		case ROM:         return "ROM";
		case TERMINAL:    return "Terminal";
		case TRACE:       return "TRACE";
		case USB:         return "Usb";
		case RTC:         return "Rtc";
		case PLATFORM:    return "Platform";
		case PIN_STATE:   return "Pin_state";
		case PIN_CONTROL: return "Pin_control";
		case VM:          return "VM";
		case PD:          return "PD";
		case PLAY:        return "Play";
		case RECORD:      return "Record";
		case UNDEFINED:   break;
		}
		return "undefined";
	}

	struct Types;

	static inline Resource from_node_type(Types const &, Node_type const &);

	static inline Resource from_node(Types const &, Node const &);
};


struct Depot_deploy::Resource::Types
{
	struct Entry;
	using Dict = Dictionary<Entry, Node_type>;

	struct Entry : Dict::Element
	{
		Type const type;

		Entry(Dict &dict, Node_type const &node_type, Type type)
		: Dict::Element(dict, node_type), type(type) { }
	};

	Dictionary<Entry, Node_type> dict { };

	Entry const _entries[30] {
		{ dict, "audio_in",    AUDIO_IN    }, /* deprecated */
		{ dict, "audio_out",   AUDIO_OUT   }, /* deprecated */
		{ dict, "block",       BLOCK       },
		{ dict, "event",       EVENT       },
		{ dict, "capture",     CAPTURE     },
		{ dict, "fs",          FS          },
		{ dict, "file_system", FS          }, /* deprecated */
		{ dict, "nic",         NIC         },
		{ dict, "uplink",      UPLINK      },
		{ dict, "gui",         GUI         },
		{ dict, "gpu",         GPU         },
		{ dict, "log",         LOG         },
		{ dict, "rm",          RM          },
		{ dict, "i2c",         I2C         },
		{ dict, "io_mem",      IO_MEM      },
		{ dict, "io_port",     IO_PORT     },
		{ dict, "irq",         IRQ         },
		{ dict, "report",      REPORT      },
		{ dict, "rom",         ROM         },
		{ dict, "terminal",    TERMINAL    },
		{ dict, "trace",       TRACE       },
		{ dict, "usb",         USB         },
		{ dict, "rtc",         RTC         },
		{ dict, "platform",    PLATFORM    },
		{ dict, "pin_state",   PIN_STATE   },
		{ dict, "pin_control", PIN_CONTROL },
		{ dict, "vm",          VM          },
		{ dict, "pd",          PD          },
		{ dict, "play",        PLAY        },
		{ dict, "record",      RECORD      },
	};
};


Depot_deploy::Resource
Depot_deploy::Resource::from_node_type(Types const &types, Node_type const &node_type)
{
	return types.dict.with_element(node_type,
		[&] (Types::Entry const &e) { return Resource { e.type }; },
		[&]                         { return Resource { UNDEFINED }; });
}


Depot_deploy::Resource
Depot_deploy::Resource::from_node(Types const &types, Node const &node)
{
	/* deprecated */
	if (node.type() == "service") {
		using Name = String<32>;
		Name const name = node.attribute_value("name", Name());
		Resource result { UNDEFINED };
		types.dict.for_each([&] (Types::Entry const &e) {
			Resource const resource { e.type };
			if (name == resource.service_name())
				result = resource; });
		return result;
	}

	return from_node_type(types, node.type());
}

#endif /* _RESOURCE_H_ */
