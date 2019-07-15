/*
 * \brief  System RTC server
 * \author Josef Soentgen
 * \date   2019-07-15
 */

/*
 * Copyright (C) 2019 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <base/attached_rom_dataspace.h>
#include <base/component.h>
#include <base/heap.h>
#include <base/registry.h>
#include <root/component.h>
#include <rtc_session/connection.h>
#include <timer_session/connection.h>

/* local includes */
namespace Contrib {
#include <tm.h>
}; /* Contrib */


namespace Util {

	Genode::int64_t convert(Rtc::Timestamp const &ts)
	{
		Contrib::tm tm;
		Genode::memset(&tm, 0, sizeof(Contrib::tm));
		tm.tm_sec  = ts.second;
		tm.tm_min  = ts.minute;
		tm.tm_hour = ts.hour;
		tm.tm_mday = ts.day;
		tm.tm_mon  = ts.month - 1;
		tm.tm_year = ts.year - 1900;

		return Contrib::tm_to_secs(&tm);
	}

	Rtc::Timestamp convert(Genode::int64_t t)
	{
		Contrib::tm tm;
		Genode::memset(&tm, 0, sizeof(Contrib::tm));

		int const err = Contrib::secs_to_tm((long long)t, &tm);
		if (err) { Genode::warning("could not convert timestamp"); }

		return Rtc::Timestamp {
			.microsecond = 0,
			.second      = err ?    0 : (unsigned)tm.tm_sec,
			.minute      = err ?    0 : (unsigned)tm.tm_min,
			.hour        = err ?    0 : (unsigned)tm.tm_hour,
			.day         = err ?    1 : (unsigned)tm.tm_mday,
			.month       = err ?    1 : (unsigned)tm.tm_mon + 1,
			.year        = err ? 1970 : (unsigned)tm.tm_year + 1900
		};
	}

	struct Point_in_time
	{
		Genode::int64_t rtc_seconds;
		Genode::int64_t curr_seconds;
	};

	Rtc::Timestamp generate(Point_in_time const &p, Genode::uint64_t secs)
	{
		Genode::int64_t const s = ((Genode::int64_t)secs - p.curr_seconds) + p.rtc_seconds;
		return convert(s);
	}

} /* namespace Util */


namespace Rtc {
	using namespace Genode;

	struct Time;
	struct Session_component;
	struct Root;
	struct Main;
} /* namespace Rtc */


struct Rtc::Time
{
	Env &_env;

	Signal_context_capability _notify_sigh;

	Timer::Connection _timer { _env };
	Rtc::Connection _rtc     { _env };

	Util::Point_in_time _time_base { };

	void _update_time(Timestamp const &ts)
	{
		_time_base.rtc_seconds  = Util::convert(ts);
		_time_base.curr_seconds = _timer.curr_time().trunc_to_plain_ms().value / 1000;
		if (_notify_sigh.valid()) {
			Signal_transmitter(_notify_sigh).submit();
		}
	}

	void _handle_rtc_set()
	{
		Timestamp ts = _rtc.current_time();
		log("Set RTC base from RTC driver to ", ts);
		_update_time(ts);
	}

	Signal_handler<Time> _rtc_set_sigh {
		_env.ep(), *this, &Time::_handle_rtc_set };

	Attached_rom_dataspace _config_rom { _env, "config" };
	bool const _set_rtc {
		_config_rom.xml().attribute_value("allow_setting_rtc", false) };

	Constructible<Attached_rom_dataspace> _set_rtc_rom { };

	Signal_handler<Time> _set_rtc_sigh {
		_env.ep(), *this, &Time::_handle_set_rtc_rom };

	void _handle_set_rtc_rom()
	{
		_set_rtc_rom->update();

		if (!_set_rtc_rom->valid()) { return; }

		Genode::Xml_node node = _set_rtc_rom->xml();

		bool const complete = node.has_attribute("year")
			&& node.has_attribute("month")
			&& node.has_attribute("day")
			&& node.has_attribute("hour")
			&& node.has_attribute("minute")
			&& node.has_attribute("second");

		if (!complete) {
			Genode::warning("set_rtc: ignoring incomplete RTC update");
			return;
		}

		Timestamp ts { };

		ts.second = node.attribute_value("second", 0u);
		if (ts.second > 59) {
			Genode::error("set_rtc: second invalid");
			return;
		}

		ts.minute = node.attribute_value("minute", 0u);
		if (ts.minute > 59) {
			Genode::error("set_rtc: minute invalid");
			return;
		}

		ts.hour = node.attribute_value("hour", 0u);
		if (ts.hour > 23) {
			Genode::error("set_rtc: hour invalid");
			return;
		}

		ts.day = node.attribute_value("day", 1u);
		if (ts.day > 31 || ts.day == 0) {
			Genode::error("set_rtc: day invalid");
			return;
		}

		ts.month = node.attribute_value("month", 1u);
		if (ts.month > 12 || ts.month == 0) {
			Genode::error("set_rtc: month invalid");
			return;
		}

		ts.year = node.attribute_value("year", 2019u);

		ts.microsecond = 0;

		log("Set RTC base from 'set_rtc' ROM to ", ts);
		_update_time(ts);
	}

	Time(Env &env, Signal_context_capability notify_sigh)
	: _env { env }, _notify_sigh { notify_sigh }
	{
		_rtc.set_sigh(_rtc_set_sigh);
		_handle_rtc_set();

		if (_set_rtc) {
			_set_rtc_rom.construct(env, "set_rtc");
			_set_rtc_rom->sigh(_set_rtc_sigh);
		}
	}

	Timestamp current_time()
	{
		Milliseconds curr_ms = _timer.curr_time().trunc_to_plain_ms();
		return Util::generate(_time_base, curr_ms.value / 1000);
	}
};


struct Rtc::Session_component : public Genode::Rpc_object<Session>
{
	Rtc::Time &_time;

	Signal_context_capability _set_sig_cap { };

	Session_component(Rtc::Time &time) : _time { time } { }

	virtual ~Session_component() { }

	void set_sigh(Signal_context_capability sigh) override
	{
		_set_sig_cap = sigh;
	}

	Timestamp current_time() override
	{
		return _time.current_time();
	}

	void notify_client()
	{
		if (_set_sig_cap.valid()) {
			Signal_transmitter(_set_sig_cap).submit();
		}
	}
};


class Rtc::Root : public Genode::Root_component<Session_component>
{
	private:

		Signal_handler<Rtc::Root> _set_sigh;

		void _handle_set_signal()
		{
			_sessions.for_each([&] (Session_component &session) {
				session.notify_client();
			});
		}

		Time _time;

		Registry<Registered<Session_component> > _sessions { };

	protected:

		Session_component *_create_session(const char *) override
		{
			return new (md_alloc())
				Registered<Session_component>(_sessions, _time);
		}

	public:

		Root(Env &env, Allocator &md_alloc)
		:
			Genode::Root_component<Session_component> { &env.ep().rpc_ep(), &md_alloc },
			_set_sigh { env.ep(), *this, &Rtc::Root::_handle_set_signal },
			_time { env, _set_sigh }
		{ }
};


struct Rtc::Main
{
	Env &_env;

	Sliced_heap _sliced_heap { _env.ram(), _env.rm() };

	Root _root { _env, _sliced_heap };

	Main(Env &env) : _env(env)
	{
		env.parent().announce(env.ep().manage(_root));
	}
};


void Component::construct(Genode::Env &env)
{
	static Rtc::Main main(env);
}
