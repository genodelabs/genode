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
	public:

		typedef String<100> Name;

	private:

		Name const _name;

		size_t const _buffer_size;

		bool _enabled = false;

		struct Connection
		{
			Report::Connection report;
			Attached_dataspace ds = { report.dataspace() };

			Connection(char const *name, size_t buffer_size)
			: report(name, buffer_size) { }
		};

		Lazy_volatile_object<Connection> _conn;

		/**
		 * Return size of report buffer
		 */
		size_t _size() const { return _enabled ? _conn->ds.size() : 0; }

		/**
		 * Return pointer to report buffer
		 */
		char *_base() { return _enabled ? _conn->ds.local_addr<char>() : 0; }

	public:

		Reporter(char const *report_name, size_t buffer_size = 4096)
		: _name(report_name), _buffer_size(buffer_size) { }

		/**
		 * Enable or disable reporting
		 */
		void enabled(bool enabled)
		{
			if (enabled == _enabled) return;

			if (enabled)
				_conn.construct(_name.string(), _buffer_size);
			else
				_conn.destruct();

			_enabled = enabled;
		}

		bool is_enabled() const { return _enabled; }

		Name name() const { return _name; }

		/**
		 * Clear report buffer
		 */
		void clear() { memset(_base(), 0, _size()); }

		/**
		 * Report data buffer
		 *
		 * \param data    data buffer
		 * \param length  number of bytes to report
		 */
		void report(void const *data, size_t length)
		{
			void *base = _base();

			if (!base || length > _size())
				return;

			memcpy(base, data, length);
			_conn->report.submit(length);
		}

		/**
		 * XML generator targeting a reporter
		 */
		struct Xml_generator : public Genode::Xml_generator
		{
			template <typename FUNC>
			Xml_generator(Reporter &reporter, FUNC const &func)
			:
				Genode::Xml_generator(reporter._base(),
				                      reporter._size(),
				                      reporter._name.string(),
				                      func)
			{
				reporter._conn->report.submit(used());
			}
		};
};

#endif /* _INCLUDE__OS__REPORTER_H_ */
