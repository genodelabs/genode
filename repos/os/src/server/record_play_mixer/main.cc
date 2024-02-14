/*
 * \brief  Audio mixer
 * \author Norman Feske
 * \date   2023-12-13
 */

/*
 * Copyright (C) 2023 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <base/component.h>
#include <base/heap.h>
#include <base/attached_rom_dataspace.h>
#include <base/sleep.h>
#include <os/reporter.h>
#include <os/session_policy.h>
#include <timer_session/connection.h>

/* local includes */
#include <play_session.h>
#include <record_session.h>
#include <mix_signal.h>

namespace Mixer { struct Main; }


struct Mixer::Main : Record_session::Operations, Play_session::Operations
{
	Env &_env;

	Attached_rom_dataspace _config { _env, "config" };

	Heap _heap { _env.ram(), _env.rm() };

	Timer::Connection _timer { _env };

	Expanding_reporter _state_reporter { _env, "state", "state" };

	Play_sessions   _play_sessions   { };
	Record_sessions _record_sessions { };

	Play_root   _play_root   { _env, _heap, _play_sessions,   *this };
	Record_root _record_root { _env, _heap, _record_sessions, *this };

	using Config_version = String<32>;
	Config_version _version { };

	Constructible<Clock> _clock_from_config { };

	Time_window_scheduler::Config _global_record_config { };
	unsigned                      _global_play_jitter_us { };

	/**
	 * Record_session::Operations
	 */
	Clock current_clock_value() override
	{
		if (_clock_from_config.constructed())
			return *_clock_from_config;

		return Clock { unsigned(_timer.elapsed_us()) };
	}

	List_model<Audio_signal> _audio_signals { };

	/**
	 * Record_session::Operations
	 */
	void bind_sample_producers_to_record_sessions() override
	{
		_record_sessions.for_each([&] (Record_session &record_session) {

			record_session.release_sample_producer();

			using Name = Audio_signal::Name;
			with_matching_policy(record_session.label(), _config.xml(),
				[&] (Xml_node const &policy) {
					record_session.apply_config(policy, _global_record_config);
					Name const name = policy.attribute_value("record", Name());
					_audio_signals.for_each([&] (Audio_signal &audio_signal) {
						if (audio_signal.name == name)
							record_session.assign_sample_producer(audio_signal); });
				},
				[&] {
					log("no policy for session ", record_session.label());
				});
		});
	}

	/**
	 * Play_session::Operations
	 */
	void bind_play_sessions_to_audio_signals() override
	{
		_play_sessions.for_each([&] (Play_session &play_session) {
			play_session.global_jitter_us(_global_play_jitter_us); });

		_audio_signals.for_each([&] (Audio_signal &audio_signal) {
			audio_signal.bind_inputs(_audio_signals, _play_sessions); });
	}

	/**
	 * Play_session::Operations
	 */
	void wakeup_record_clients() override
	{
		_record_sessions.for_each([&] (Record_session &record_session) {
			record_session.wakeup(); });
	}

	void _generate_state_report(Xml_generator &xml) const
	{
		if (_clock_from_config.constructed())
			xml.attribute("clock_value", _clock_from_config->us());
		(void)xml;
	}

	void _update_state_report()
	{
		_state_reporter.generate([&] (Xml_generator &xml) {
			xml.attribute("version", _version);
			_generate_state_report(xml);
		});
	}

	void _handle_config()
	{
		_config.update();
		Xml_node const config = _config.xml();

		double const default_jitter_ms = config.attribute_value("jitter_ms", 1.0);
		_global_record_config = {
			.period_us = us_from_ms_attr(config, "record_period_ms", 5.0),
			.jitter_us = us_from_ms_attr(config, "record_jitter_ms", default_jitter_ms),
		};
		_global_play_jitter_us = us_from_ms_attr(config, "play_jitter_ms", default_jitter_ms);

		_version = config.attribute_value("version", _version);

		_clock_from_config.destruct();
		if (config.has_attribute("clock_value"))
			_clock_from_config.construct(config.attribute_value("clock_value", 0u));

		_audio_signals.update_from_xml(config,

			/* create */
			[&] (Xml_node const &node) -> Audio_signal & {

				if (node.has_type(Audio_signal::mix_type_name))
					return *new (_heap) Mix_signal(node, _heap);

				error("unable to create signal: ", node);
				sleep_forever(); /* should never be reachable */
			},

			/* destroy */
			[&] (Audio_signal &audio_signal) {
				destroy(_heap, &audio_signal); },

			/* update */
			[&] (Audio_signal &audio_signal, Xml_node const &node) {
				audio_signal.update(node); }
		);

		bind_play_sessions_to_audio_signals();
		bind_sample_producers_to_record_sessions();

		_update_state_report();
	}

	struct Timer_count { unsigned value; };

	Timer_count _count { };

	void _handle_timer()
	{
		_count.value++;
	}

	Timer_count _once_in_a_while_triggered { };

	bool once_in_a_while() override
	{
		if (_count.value == _once_in_a_while_triggered.value)
			return false;

		_once_in_a_while_triggered = _count;
		return true;
	}

	Signal_handler<Main> _config_handler
		{ _env.ep(), *this, &Main::_handle_config };

	Signal_handler<Main> _timer_handler
		{ _env.ep(), *this, &Main::_handle_timer };

	Main(Genode::Env &env) : _env(env)
	{
		_config.sigh(_config_handler);
		_handle_config();

		_timer.sigh(_timer_handler);
		_timer.trigger_periodic(1000*1000);

		_env.parent().announce(_env.ep().manage(_play_root));
		_env.parent().announce(_env.ep().manage(_record_root));
	}
};


void Component::construct(Genode::Env &env) { static Mixer::Main inst(env); }
