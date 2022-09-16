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

/* local includes */
#include <cached_timer.h>

/* Genode */
#include <os/reporter.h>

namespace Genode {

	class Xml_node;
	class Env;
}

namespace Net {

	class Domain_dict;
	class Report;
	class Quota;
}


struct Net::Quota
{
	Genode::size_t ram { 0 };
	Genode::size_t cap { 0 };
};


class Net::Report
{
	private:

		bool                      const &_verbose;
		bool                      const  _config;
		bool                      const  _config_triggers;
		bool                      const  _bytes;
		bool                      const  _stats;
		bool                      const  _dropped_fragm_ipv4;
		bool                      const  _link_state;
		bool                      const  _link_state_triggers;
		bool                      const  _quota;
		Quota                     const &_shared_quota;
		Genode::Pd_session              &_pd;
		Genode::Reporter                &_reporter;
		Domain_dict                     &_domains;
		Timer::Periodic_timeout<Report>  _timeout;
		Genode::Signal_transmitter       _signal_transmitter;

		void _handle_report_timeout(Genode::Duration);

	public:

		struct Empty : Genode::Exception { };

		Report(bool                              const &verbose,
		       Genode::Xml_node                  const  node,
		       Cached_timer                            &timer,
		       Domain_dict                             &domains,
		       Quota                             const &shared_quota,
		       Genode::Pd_session                      &pd,
		       Genode::Reporter                        &reporter,
		       Genode::Signal_context_capability const &signal_cap);

		void handle_config();

		void handle_interface_link_state();

		void generate();


		/***************
		 ** Accessors **
		 ***************/

		bool config()               const { return _config; }
		bool bytes()                const { return _bytes; }
		bool stats()                const { return _stats; }
		bool dropped_fragm_ipv4()   const { return _dropped_fragm_ipv4; }
		bool link_state()           const { return _link_state; }
		bool link_state_triggers()  const { return _link_state_triggers; }
};

#endif /* _REPORT_H_ */
