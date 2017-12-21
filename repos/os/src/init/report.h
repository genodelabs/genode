/*
 * \brief  Report configuration
 * \author Norman Feske
 * \date   2017-01-16
 */

/*
 * Copyright (C) 2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _SRC__INIT__REPORT_H_
#define _SRC__INIT__REPORT_H_

#include <util/noncopyable.h>
#include <util/xml_node.h>

namespace Init {
	struct Report_update_trigger;
	struct Report_detail;
}


class Init::Report_detail : Genode::Noncopyable
{
	private:

		bool _children     = false;
		bool _ids          = false;
		bool _requested    = false;
		bool _provided     = false;
		bool _session_args = false;
		bool _child_ram    = false;
		bool _child_caps   = false;
		bool _init_ram     = false;
		bool _init_caps    = false;

	public:

		Report_detail() { }

		Report_detail(Genode::Xml_node report)
		{
			_children     = true;
			_ids          = report.attribute_value("ids",          false);
			_requested    = report.attribute_value("requested",    false);
			_provided     = report.attribute_value("provided",     false);
			_session_args = report.attribute_value("session_args", false);
			_child_ram    = report.attribute_value("child_ram",    false);
			_child_caps   = report.attribute_value("child_caps",   false);
			_init_ram     = report.attribute_value("init_ram",     false);
			_init_caps    = report.attribute_value("init_caps",    false);
		}

		bool children()     const { return _children;     }
		bool ids()          const { return _ids;          }
		bool requested()    const { return _requested;    }
		bool provided()     const { return _provided;     }
		bool session_args() const { return _session_args; }
		bool child_ram()    const { return _child_ram;    }
		bool child_caps()   const { return _child_caps;   }
		bool init_ram()     const { return _init_ram;     }
		bool init_caps()    const { return _init_caps;    }
};


struct Init::Report_update_trigger : Interface
{
	virtual void trigger_report_update() = 0;
};

#endif /* _SRC__INIT__REPORT_H_ */
