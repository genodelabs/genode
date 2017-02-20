/*
 * \brief  Utility for status reporting
 * \author Norman Feske
 * \date   2014-01-07
 */

/*
 * Copyright (C) 2014-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__OS__REPORTER_H_
#define _INCLUDE__OS__REPORTER_H_

#include <util/reconstructible.h>
#include <base/attached_dataspace.h>
#include <report_session/connection.h>
#include <util/xml_generator.h>


namespace Genode { class Reporter; }


class Genode::Reporter : Noncopyable
{
	public:

		typedef String<100> Name;

	private:

		Name const _xml_name;
		Name const _label;

		size_t const _buffer_size;

		bool _enabled = false;

		struct Connection
		{
			Report::Connection report;
			Attached_dataspace ds = { *env_deprecated()->rm_session(), report.dataspace() };

			Connection(char const *name, size_t buffer_size)
			: report(false, name, buffer_size) { }
		};

		Constructible<Connection> _conn;

		/**
		 * Return size of report buffer
		 */
		size_t _size() const { return _enabled ? _conn->ds.size() : 0; }

		/**
		 * Return pointer to report buffer
		 */
		char *_base() { return _enabled ? _conn->ds.local_addr<char>() : 0; }

	public:

		Reporter(Env &env, char const *xml_name, char const *label = nullptr,
		         size_t buffer_size = 4096)
		:
			_xml_name(xml_name), _label(label ? label : xml_name),
			_buffer_size(buffer_size)
		{ }

		/**
		 * Constructor
		 *
		 * \deprecated
		 * \noapi
		 */
		Reporter(char const *xml_name, char const *label = nullptr,
		         size_t buffer_size = 4096) __attribute__((deprecated))
		:
			_xml_name(xml_name), _label(label ? label : xml_name),
			_buffer_size(buffer_size)
		{ }

		/**
		 * Enable or disable reporting
		 */
		void enabled(bool enabled)
		{
			if (enabled == _enabled) return;

			if (enabled)
				_conn.construct(_label.string(), _buffer_size);
			else
				_conn.destruct();

			_enabled = enabled;
		}

		/**
		 * Return true if reporter is enabled
		 */
		bool enabled() const { return _enabled; }

		/**
		 * Return true if reporter is enabled
		 *
		 * \noapi
		 * \deprecated  use 'enabled' instead
		 */
		bool is_enabled() const { return enabled(); }

		Name name() const { return _label; }

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
				                      reporter._xml_name.string(),
				                      func)
			{
				reporter._conn->report.submit(used());
			}
		};
};

#endif /* _INCLUDE__OS__REPORTER_H_ */
