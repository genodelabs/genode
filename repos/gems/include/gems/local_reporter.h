/*
 * \brief  Utility for producing reports to a report session
 * \author Norman Feske
 * \date   2014-02-14
 */

/*
 * Copyright (C) 2014-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__GEMS__LOCAL_REPORTER_H_
#define _INCLUDE__GEMS__LOCAL_REPORTER_H_

/* Genode includes */
#include <base/attached_dataspace.h>
#include <util/xml_generator.h>
#include <report_session/client.h>

class Local_reporter
{
	private:

		Local_reporter(Local_reporter const &);
		Local_reporter &operator = (Local_reporter const &);

		Report::Session_client _session;

		Genode::Attached_dataspace _ds;

		char const *_name;

	public:

		Local_reporter(Genode::Region_map &rm, char const *name,
		               Genode::Capability<Report::Session> session_cap)
		:
			_session(session_cap), _ds(rm, _session.dataspace()), _name(name)
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

#endif /* _INCLUDE__GEMS__LOCAL_REPORTER_H_ */
