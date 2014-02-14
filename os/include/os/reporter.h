/*
 * \brief  Utility for status reporting
 * \author Norman Feske
 * \date   2014-01-07
 */

/*
 * Copyright (C) 2014 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__OS__REPORTER_H_
#define _INCLUDE__OS__REPORTER_H_

#include <util/volatile_object.h>
#include <os/attached_dataspace.h>
#include <report_session/connection.h>
#include <util/xml_generator.h>


namespace Genode { class Reporter; }


class Genode::Reporter : Noncopyable
{
	private:

		String<100> const _name;

		bool _enabled = false;

		struct Connection
		{
			Report::Connection report;
			Attached_dataspace ds = { report.dataspace() };

			Connection(char const *name) : report(name) { }
		};

		Lazy_volatile_object<Connection> _conn;

	public:

		Reporter(char const *report_name) : _name(report_name) { }

		/**
		 * Enable or disable reporting
		 */
		void enabled(bool enabled)
		{
			if (enabled == _enabled) return;

			if (enabled)
				_conn.construct(_name.string());
			else
				_conn.destruct();

			_enabled = enabled;
		}

		bool is_enabled() const { return _enabled; }

		/**
		 * Return size of report buffer
		 */
		size_t size() const { return _enabled ? _conn->ds.size() : 0; }

		/**
		 * Return pointer to report buffer
		 */
		char *base() { return _enabled ? _conn->ds.local_addr<char>() : 0; }

		/**
		 * XML generator targeting a reporter
		 */
		struct Xml_generator : public Genode::Xml_generator
		{
			template <typename FUNC>
			Xml_generator(Reporter &reporter, FUNC const &func)
			:
				Genode::Xml_generator(reporter.base(),
				                      reporter.size(),
				                      reporter._name.string(),
				                      func)
			{
				reporter._conn->report.submit(used());
			}
		};
};


#endif /* _INCLUDE__OS__REPORTER_H_ */
