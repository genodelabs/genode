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

#include <util/retry.h>
#include <util/xml_node.h>
#include <util/reconstructible.h>
#include <base/attached_dataspace.h>
#include <report_session/connection.h>
#include <util/xml_generator.h>

namespace Genode {
	class Reporter;
	class Expanding_reporter;
}


class Genode::Reporter
{
	public:

		typedef String<100> Name;

	private:

		Env &_env;

		Name const _xml_name;
		Name const _label;

		size_t const _buffer_size;

		bool _enabled = false;

		struct Connection
		{
			Report::Connection report;
			Attached_dataspace ds;

			Connection(Env &env, char const *name, size_t buffer_size)
			: report(env, name, buffer_size), ds(env.rm(), report.dataspace())
			{ }
		};

		Constructible<Connection> _conn { };

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
			_env(env), _xml_name(xml_name), _label(label ? label : xml_name),
			_buffer_size(buffer_size)
		{ }

		/**
		 * Enable or disable reporting
		 */
		void enabled(bool enabled)
		{
			if (enabled == _enabled) return;

			if (enabled)
				_conn.construct(_env, _label.string(), _buffer_size);
			else
				_conn.destruct();

			_enabled = enabled;
		}

		/**
		 * Return true if reporter is enabled
		 */
		bool enabled() const { return _enabled; }

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
				if (reporter.enabled())
					reporter._conn->report.submit(used());
			}
		};
};


/**
 * Reporter that increases the report buffer capacity on demand
 *
 * This convenience wrapper of the 'Reporter' alleviates the need to handle
 * 'Xml_generator::Buffer_exceeded' exceptions manually. In most cases, the
 * only reasonable way to handle such an exception is upgrading the report
 * buffer as done by this class. Furthermore, in contrast to the regular
 * 'Reporter', which needs to be 'enabled', the 'Expanding_reporter' is
 * implicitly enabled at construction time.
 */
class Genode::Expanding_reporter
{
	public:

		typedef Session_label Label;
		typedef String<64>    Node_type;

		struct Initial_buffer_size { size_t value; };

	private:

		Env &_env;

		Node_type const _type;
		Label     const _label;

		Constructible<Reporter> _reporter { };

		size_t _buffer_size;

		void _construct()
		{
			_reporter.construct(_env, _type.string(), _label.string(), _buffer_size);
			_reporter->enabled(true);
		}

		void _increase_report_buffer()
		{
			_buffer_size += 4096;
			_construct();
		}

	public:

		Expanding_reporter(Env &env, Node_type const &type, Label const &label,
		                   Initial_buffer_size const size = { 4096 })
		: _env(env), _type(type), _label(label), _buffer_size(size.value)
		{ _construct(); }

		template <typename FN>
		void generate(FN const &fn)
		{
			retry<Xml_generator::Buffer_exceeded>(

				[&] () {
					Reporter::Xml_generator
						xml(*_reporter, [&] () { fn(xml); }); },

				[&] () { _increase_report_buffer(); }
			);
		}

		void generate(Xml_node node)
		{
			retry<Xml_generator::Buffer_exceeded>(

				[&] () {
					node.with_raw_node([&] (char const *start, size_t length) {
						_reporter->report(start, length); }); },

				[&] () { _increase_report_buffer(); }
			);
		}
};

#endif /* _INCLUDE__OS__REPORTER_H_ */
