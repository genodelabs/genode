/*
 * \brief  RTC server
 * \author Christian Helmuth
 * \date   2015-01-06
 */

/*
 * Copyright (C) 2015-2019 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode */
#include <base/attached_rom_dataspace.h>
#include <base/component.h>
#include <base/heap.h>
#include <base/registry.h>
#include <root/component.h>

#include "rtc.h"


namespace Rtc {
	using namespace Genode;

	struct Session_component;
	struct Root;
	struct Main;
}


struct Rtc::Session_component : public Genode::Rpc_object<Session>
{
	Env &_env;

	Signal_context_capability _set_sig_cap { };

	Session_component(Env &env) : _env(env) { }

	virtual ~Session_component() { }

	void set_sigh(Signal_context_capability sigh) override
	{
		_set_sig_cap = sigh;
	}

	Timestamp current_time() override
	{
		Timestamp ret = Rtc::get_time(_env);

		return ret;
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

		Env &_env;

		Registry<Registered<Session_component> > _sessions { };

	protected:

		Session_component *_create_session(const char *) override
		{
			return new (md_alloc())
				Registered<Session_component>(_sessions, _env);
		}

	public:

		Root(Env &env, Allocator &md_alloc)
		:
			Genode::Root_component<Session_component>(&env.ep().rpc_ep(), &md_alloc),
			_env(env)
		{
			/* trigger initial RTC read */
			Rtc::get_time(_env);
		}

		void notify_clients()
		{
			_sessions.for_each([&] (Session_component &session) {
				session.notify_client();
			});
		}
};


struct Rtc::Main
{
	Env &env;

	Sliced_heap sliced_heap { env.ram(), env.rm() };

	Root root { env, sliced_heap };

	Attached_rom_dataspace _config_rom { env, "config" };
	bool const _set_rtc {
		_config_rom.xml().attribute_value("allow_setting_rtc", false) };

	Constructible<Attached_rom_dataspace> _update_rom { };

	void _handle_update();

	Signal_handler<Main> _update_sigh {
		env.ep(), *this, &Main::_handle_update };

	Main(Env &env) : env(env)
	{
		if (_set_rtc) {
			_update_rom.construct(env, "set_rtc");
			_update_rom->sigh(_update_sigh);
		}

		env.parent().announce(env.ep().manage(root));
	}
};


void Rtc::Main::_handle_update()
{
	_update_rom->update();

	if (!_update_rom->valid()) { return; }

	Genode::Xml_node node = _update_rom->xml();

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

	Rtc::set_time(env, ts);

	root.notify_clients();
}


void Component::construct(Genode::Env &env) { static Rtc::Main main(env); }
