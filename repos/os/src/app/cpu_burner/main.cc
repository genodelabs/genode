/*
 * \brief  CPU burner
 * \author Norman Feske
 * \date   2015-06-30
 */

/*
 * Copyright (C) 2015-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <base/component.h>
#include <base/heap.h>
#include <base/attached_rom_dataspace.h>
#include <timer_session/connection.h>

using namespace Genode;

struct Cpu_burn : Thread
{
	List_element<Cpu_burn> _list_element { this };
	Env                   &_env;
	Blockade               _block { };
	bool volatile          _stop  { false };

	Cpu_burn(Env &env, Location const &location)
	:
		Thread(env, Name("burn_", location.xpos(), "x", location.ypos()),
		       4 * 4096, location, Weight(), env.cpu()),
		_env(env)
	{ }

	void entry() override
	{
		while (true) {
			_block.block();

			while (!_stop) { }

			_stop = false;
		}
	}
};


struct Cpu_burner
{
	typedef List<List_element<Cpu_burn> >  Thread_list;

	Env              &_env;
	Heap              _heap   { _env.ram(), _env.rm() };
	Timer::Connection _timer   { _env };
	Thread_list       _threads { };

	uint64_t          _start_ms { 0 };
	unsigned short    _percent  { 100 };
	bool              _burning  { false };

	Attached_rom_dataspace _config { _env, "config" };

	void _handle_config()
	{
		_config.update();

		if (!_config.valid())
			return;

		_percent = _config.xml().attribute_value("percent", (unsigned short)100);
		if (_percent > 100)
			_percent = 100;
	}

	Signal_handler<Cpu_burner> _config_handler {
		_env.ep(), *this, &Cpu_burner::_handle_config };

	void _handle_period()
	{
		uint64_t next_timer_ms = 1000;
		bool     stop_burner   = false;
		bool     start_burner  = false;

		if (_burning) {
			uint64_t const curr_ms   = _timer.elapsed_ms();
			uint64_t const passed_ms = curr_ms - _start_ms;

			if (_percent < 100) {
				stop_burner = (passed_ms >= 10*_percent);
				if (stop_burner)
					next_timer_ms = (100 - _percent) * 10;
				else
					next_timer_ms = 10*_percent - passed_ms;
			}
		} else
			start_burner = true;

		if (stop_burner) {
			for (auto t = _threads.first(); t; t = t->next()) {
				auto thread = t->object();
				if (!thread)
					continue;

				thread->_stop = true;
			}
			_burning = false;
		}

		if (start_burner) {
			for (auto t = _threads.first(); t; t = t->next()) {
				auto thread = t->object();
				if (!thread)
					continue;

				thread->_block.wakeup();
			}
			_burning  = true;
			_start_ms = _timer.elapsed_ms();
		}

		if (_percent < 100)
			_timer.trigger_once(next_timer_ms*1000);
	}

	Signal_handler<Cpu_burner> _period_handler {
		_env.ep(), *this, &Cpu_burner::_handle_period };

	Cpu_burner(Env &env) : _env(env)
	{
		_config.sigh(_config_handler);
		_handle_config();

		_timer.sigh(_period_handler);
		Affinity::Space space = env.cpu().affinity_space();

		for (unsigned i = 0; i < space.total(); i++) {
			Affinity::Location location = env.cpu().affinity_space().location_of_index(i);
			Cpu_burn *t = new (_heap) Cpu_burn(env, location);
			t->start();

			_threads.insert(&t->_list_element);
		}

		if (_percent < 100)
			_timer.trigger_once(1000*1000);
		else
			_handle_period();
	}
};


void Component::construct(Env &env)
{
	static Cpu_burner cpu_burner(env);
}
