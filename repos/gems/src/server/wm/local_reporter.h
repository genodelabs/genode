/*
 * \brief  Utility for producing reports to a report session
 * \author Norman Feske
 * \date   2014-02-14
 */

/*
 * Copyright (C) 2014 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _LOCAL_REPORTER_H_
#define _LOCAL_REPORTER_H_

#include <os/attached_dataspace.h>
#include <util/xml_generator.h>
#include <report_session/client.h>


namespace Wm { struct Local_reporter; }

struct Wm::Local_reporter
{
	Report::Session_client _session;

	Genode::Attached_dataspace _ds;

	char const *_name;

	Local_reporter(char const *name, Genode::Capability<Report::Session> session_cap)
	:
		_session(session_cap), _ds(_session.dataspace()), _name(name)
	{ }

	struct Xml_generator : public Genode::Xml_generator
	{
		template <typename FUNC>
		Xml_generator(Local_reporter &reporter, FUNC const &func)
		:
			Genode::Xml_generator(reporter._ds.local_addr<char>(),
			                      reporter._ds.size(),
			                      reporter._name,
			                      func)
		{
			reporter._session.submit(used());
		}
	};
};

#endif /* _LOCAL_REPORTER_H_ */
