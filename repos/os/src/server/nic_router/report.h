/*
 * \brief  Report generation unit
 * \author Martin Stein
 * \date   2016-08-24
 */

/*
 * Copyright (C) 2016-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _REPORT_H_
#define _REPORT_H_

/* Genode */
#include <timer_session/connection.h>
#include <os/reporter.h>

namespace Genode {

	class Xml_node;
	class Env;
}

namespace Net {

	class Domain_tree;
	class Report;
}


class Net::Report
{
	private:

		bool const                       _config;
		bool const                       _config_triggers;
		bool const                       _bytes;
		Genode::Reporter                &_reporter;
		Domain_tree                     &_domains;
		Timer::Periodic_timeout<Report>  _timeout;

		void _handle_report_timeout(Genode::Duration);

		void _report();

	public:

		Report(Genode::Xml_node const  node,
		       Timer::Connection      &timer,
		       Domain_tree            &domains,
		       Genode::Reporter       &reporter);

		void handle_config();


		/***************
		 ** Accessors **
		 ***************/

		bool config() const { return _config; }
		bool bytes()  const { return _bytes; }
};

#endif /* _REPORT_H_ */
