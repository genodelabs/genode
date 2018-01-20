/*
 * \brief  Component transforming core and kernel output to Genode LOG output
 * \author Alexander Boettcher
 * \date   2016-12-22
 */

/*
 * Copyright (C) 2016-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* base includes */
#include <base/component.h>
#include <log_session/connection.h>

/* os includes */
#include <base/attached_rom_dataspace.h>
#include <timer_session/connection.h>


class Log
{
	private:

		Genode::Attached_rom_dataspace _rom_ds;
		Genode::Log_connection         _log;

		char           _buffer [Genode::Log_session::MAX_STRING_LEN + 1];
		unsigned short _buf_pos { 0 };
		unsigned       _rom_pos { 0 };

		unsigned log_size() const { return _rom_ds.size() - sizeof(_rom_pos); }

		char const * char_from_rom(unsigned offset = 0) const
		{
			return _rom_ds.local_addr<char const>() + sizeof(_rom_pos) +
			       (_rom_pos + offset) % log_size();
		}

		unsigned next_pos(unsigned pos) const {
			return (pos + 1) % log_size(); }

		unsigned end_pos() const {
			return *_rom_ds.local_addr<unsigned volatile>() % log_size(); }

		void _rom_to_log(unsigned const last_pos)
		{
			unsigned up_to_pos = last_pos;

			for (; _rom_pos != next_pos(up_to_pos)
				 ; _rom_pos = next_pos(_rom_pos), up_to_pos = end_pos()) {

				char const c = *char_from_rom();

				_buffer[_buf_pos++] = c;

				if (_buf_pos + 1U < sizeof(_buffer) && c != '\n')
					continue;

				_buffer[_buf_pos] = 0;
				_log.write(Genode::Log_session::String(_buffer));
				_buf_pos = 0;
			}
		}

	public:

		Log (Genode::Env &env, char const * const rom_name,
		     char const * const log_name)
		: _rom_ds(env, rom_name), _log(env, log_name)
		{
			unsigned const pos = end_pos();

			/* initial check whether already log wrapped at least one time */
			enum { COUNT_TO_CHECK_FOR_WRAP = 8 };
			for (unsigned i = 1; i <= COUNT_TO_CHECK_FOR_WRAP; i++) {
				if (*char_from_rom(pos + i) == 0)
					continue;

				/* wrap detected, set pos behind last known pos */
				_rom_pos = next_pos(pos + 1) % log_size();
				break;
			}

			_rom_to_log(pos);
		}

		void log() { _rom_to_log(end_pos()); }
};

struct Monitor
{
	Genode::Env &env;

	Log output { env, "log", "log" };

	Timer::Connection timer { env };

	Genode::Signal_handler<Monitor> interval { env.ep(), *this, &Monitor::check };

	Monitor(Genode::Env &env) : env(env)
	{
		timer.sigh(interval);

		Genode::addr_t period_ms = 1000;

		try {
			Genode::Attached_rom_dataspace config { env, "config" };
			period_ms = config.xml().attribute_value("period_ms", 1000UL);
		} catch (...) { }

		timer.trigger_periodic(1000UL * period_ms);
	}

	void check()
	{
		output.log();
	}
};


void Component::construct(Genode::Env &env) { static Monitor output(env); }
