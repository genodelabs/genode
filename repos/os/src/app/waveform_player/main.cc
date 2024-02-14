/*
 * \brief  Waveform generator targeting play sessions
 * \author Norman Feske
 * \date   2023-12-20
 */

/*
 * Copyright (C) 2023 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <util/list_model.h>
#include <base/registry.h>
#include <base/heap.h>
#include <base/component.h>
#include <base/attached_rom_dataspace.h>
#include <timer_session/connection.h>
#include <play_session/connection.h>

namespace Waveform_player {

	using namespace Genode;

	struct Main;
}


struct Waveform_player::Main
{
	Env &_env;

	Heap _heap { _env.ram(), _env.rm() };

	Timer::Connection _timer { _env };

	Attached_rom_dataspace _config_ds { _env, "config" };

	Signal_handler<Main> _config_handler {
		_env.ep(), *this, &Main::_handle_config };

	Signal_handler<Main> _timer_handler {
		_env.ep(), *this, &Main::_handle_timer };

	struct Phase
	{
		/* value range corresponds to 0...2*pi */
		static constexpr unsigned PI2 = (1u << 16);
		uint16_t angle;
	};

	struct Waveform
	{
		static constexpr unsigned STEPS_LOG2 = 10u, STEPS = 1u << STEPS_LOG2;

		float _values[STEPS] { };

		float value(Phase phase) const
		{
			return _values[phase.angle >> (16u - STEPS_LOG2)];
		}
	};

	struct Sine_waveform : Waveform
	{
		Sine_waveform()
		{
			/* sin and cos values of 2*pi/STEPS */
			static constexpr double SINA = 0.00613588, COSA = 0.99998117;

			struct Point
			{
				double x, y;

				Point rotated() const { return { .x = x*COSA - y*SINA,
				                                 .y = y*COSA + x*SINA }; }
			} p { .x = 1.0, .y = 0.0 };

			for (unsigned i = 0; i < STEPS; i++, p = p.rotated())
				_values[i] = float(p.y);
		}
	};

	struct Square_waveform : Waveform
	{
		Square_waveform()
		{
			for (unsigned i = 0; i < STEPS; i++)
				_values[i] = (i < STEPS/2) ? -1.0f : 1.0f;
		}
	};

	struct Saw_waveform : Waveform
	{
		Saw_waveform()
		{
			for (unsigned i = 0; i < STEPS; i++)
				_values[i] = -1.0f + (float(i)*2)/STEPS;
		}
	};

	Sine_waveform   const _sine_waveform   { };
	Square_waveform const _square_waveform { };
	Saw_waveform    const _saw_waveform    { };

	enum class Wave { NONE, SINE, SQUARE, SAW };

	void with_waveform(Wave const wave, auto const &fn) const
	{
		auto waveform_ptr = [&] (Wave const wave) -> Waveform const *
		{
			if (wave == Wave::SINE)   return &_sine_waveform;
			if (wave == Wave::SQUARE) return &_square_waveform;
			if (wave == Wave::SAW)    return &_saw_waveform;
			return nullptr;
		};

		Waveform const *ptr = waveform_ptr(wave);
		if (ptr)
			fn(*ptr);
	}

	struct Channel : private List_model<Registered<Channel>>::Element
	{
		friend class List_model<Registered<Channel>>;
		friend class List<Registered<Channel>>;

		using Label = String<20>;
		Label const label;

		static Label _label_from_xml(Xml_node const &node)
		{
			return node.attribute_value("label", Label());
		}

		struct Attr
		{
			unsigned sample_rate_hz;
			double   wave_hz;
			Wave     wave;

			static Attr from_xml(Xml_node const &node, Attr const defaults)
			{
				auto wave_from_xml = [] (Xml_node const &node)
				{
					auto const attr = node.attribute_value("wave", String<16>());

					if (attr == "sine")   return Wave::SINE;
					if (attr == "square") return Wave::SQUARE;
					if (attr == "saw")    return Wave::SAW;
					if (attr == "")       return Wave::SINE;

					warning("unsupported waveform '", attr, "'");
					return Wave::NONE;
				};
				return Attr {
					.sample_rate_hz = node.attribute_value("sample_rate_hz", defaults.sample_rate_hz),
					.wave_hz        = node.attribute_value("hz",             defaults.wave_hz),
					.wave           = wave_from_xml(node)
				};
			}

			bool ready_to_play() const { return wave_hz && sample_rate_hz; }
		};

		Play::Connection _play;

		Attr  _attr  { };
		Phase _phase { };

		Phase _phase_increment() const
		{
			return { uint16_t(uint32_t(_attr.wave_hz*Phase::PI2)/_attr.sample_rate_hz) };
		}

		unsigned _samples_per_period(unsigned period_ms) const
		{
			return (period_ms*_attr.sample_rate_hz)/1000;
		}

		void _produce_samples(Waveform const &waveform, Phase increment,
		                      unsigned num_samples, auto const fn)
		{
			for (unsigned i = 0; i < num_samples; i++) {
				fn(waveform.value(_phase));
				_phase.angle += increment.angle;
			}
		}

		Channel(Env &env, Xml_node const &node)
		:
			label(_label_from_xml(node)), _play(env, label)
		{ }

		virtual ~Channel() { };

		bool ready_to_play() const { return _attr.ready_to_play(); }

		Play::Time_window play(Main const &main, Play::Time_window previous, unsigned period_ms)
		{
			Play::Duration const duration { .us = period_ms*1000 };
			unsigned       const num_samples = _samples_per_period(period_ms);
			Phase          const increment   = _phase_increment();

			return _play.schedule_and_enqueue(previous, duration, [&] (auto &submit) {
				main.with_waveform(_attr.wave, [&] (Waveform const &waveform) {
					_produce_samples(waveform, increment, num_samples,
					                 [&] (float v) { submit(v); }); }); });
		}

		void play_at(Main const &main, Play::Time_window tw, unsigned period_ms)
		{
			unsigned const num_samples = _samples_per_period(period_ms);
			Phase    const increment   = _phase_increment();

			_play.enqueue(tw, [&] (auto &submit) {
				main.with_waveform(_attr.wave, [&] (Waveform const &waveform) {
					_produce_samples(waveform, increment, num_samples,
					                 [&] (float v) { submit(v); }); }); });
		}

		void stop() { _play.stop(); }

		void update(Xml_node const &node, Attr const defaults)
		{
			_attr = Attr::from_xml(node, defaults);
		};

		/*
		 * List_model::Element
		 */
		static bool type_matches(Xml_node const &node)
		{
			return node.has_type("play");
		}

		bool matches(Xml_node const &node) const
		{
			return _label_from_xml(node) == label;
		}
	};

	List_model<Registered<Channel>> _channels { };

	Registry<Registered<Channel>> _channel_registry { }; /* for non-const for_each */

	struct Config
	{
		unsigned period_ms;

		Channel::Attr channel_defaults;

		static Config from_xml(Xml_node const &config)
		{
			return {
				.period_ms = config.attribute_value("period_ms", 10u),
				.channel_defaults = Channel::Attr::from_xml(config, {
					.sample_rate_hz = 44100u,
					.wave_hz        = 1000.0,
					.wave           = Wave::SINE
				}) };
		}
	};

	Config _config { };

	Play::Time_window _time_window { };

	void _play_channels()
	{
		bool const was_playing = ( _time_window.start != _time_window.end );

		/*
		 * The first channel drives the time window that is reused for all
		 * other channels to attain time-synchronized data.
		 */
		bool first   = true;
		bool playing = false;
		_channel_registry.for_each([&] (Registered<Channel> &channel) {
			if (channel.ready_to_play()) {
				playing = true;
				if (first)
					_time_window = channel.play(*this, _time_window, _config.period_ms);
				else
					channel.play_at(*this, _time_window, _config.period_ms);
				first = false;
			}
		});

		if (was_playing && !playing) {
			_channel_registry.for_each([&] (Registered<Channel> &channel) {
				channel.stop(); });
			_time_window = { };
		}
	}

	void _handle_timer() { _play_channels(); }

	void _handle_config()
	{
		_config_ds.update();
		Xml_node const &config = _config_ds.xml();

		_config = Config::from_xml(config);

		_channels.update_from_xml(config,
			[&] (Xml_node const &node) -> Registered<Channel> & {
				return *new (_heap)
					Registered<Channel>(_channel_registry, _env, node); },
			[&] (Registered<Channel> &channel) {
				destroy(_heap, &channel); },
			[&] (Channel &channel, Xml_node const &node) {
				channel.update(node, _config.channel_defaults); }
		);

		if (_config.period_ms)
			_timer.trigger_periodic(_config.period_ms*1000);
	}

	Main(Env &env) : _env(env)
	{
		_timer.sigh(_timer_handler);
		_config_ds.sigh(_config_handler);
		_handle_config();
	}
};


void Component::construct(Genode::Env &env) { static Waveform_player::Main main(env); }
