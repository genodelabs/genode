/*
 * \brief  Expose recorded data as ROM module
 * \author Norman Feske
 * \date   2023-01-16
 */

/*
 * Copyright (C) 2023 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <base/env.h>
#include <base/component.h>
#include <base/attached_rom_dataspace.h>
#include <base/heap.h>
#include <base/registry.h>
#include <util/list_model.h>
#include <record_session/connection.h>
#include <timer_session/connection.h>
#include <os/dynamic_rom_session.h>
#include <root/component.h>

namespace Record_rom {
	using namespace Genode;
	struct Main;
}


struct Record_rom::Main : Dynamic_rom_session::Xml_producer
{
	Env &_env;

	unsigned _period_ms { };

	Heap _heap { _env.ram(), _env.rm() };

	Timer::Connection _timer { _env };

	Attached_rom_dataspace _config { _env, "config" };

	Signal_handler<Main> _timer_handler  { _env.ep(), *this, &Main::_handle_timer };
	Signal_handler<Main> _wakeup_handler { _env.ep(), *this, &Main::_handle_wakeup };
	Signal_handler<Main> _config_handler { _env.ep(), *this, &Main::_handle_config };

	using Samples_ptr = Record::Connection::Samples_ptr;

	struct Captured_audio
	{
		static constexpr unsigned SIZE_LOG2 = 10, SIZE = 1 << SIZE_LOG2, MASK = SIZE - 1;

		float _samples[SIZE] { };

		unsigned _pos   = 0;
		unsigned _count = 0;

		void insert(float value)
		{
			_pos = (_pos + 1) & MASK;
			_samples[_pos] = value;
			_count = min(SIZE, _count + 1);
		}

		void insert(Samples_ptr const &samples)
		{
			for (unsigned i = 0; i < samples.num_samples; i++)
				insert(samples.start[i]);
		}

		float past_value(unsigned past) const
		{
			return _samples[(_pos - past) & MASK];
		}

		unsigned count() const { return _count; }
	};

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

			static Attr from_xml(Xml_node const &node, Attr const defaults)
			{
				return Attr {
					.sample_rate_hz = node.attribute_value("sample_rate_hz", defaults.sample_rate_hz),
				};
			}
		};

		Attr _attr { };

		Record::Connection _record;

		Captured_audio _capture { };

		Channel(Env &env, Xml_node const &node, Signal_context_capability wakeup_sigh)
		:
			label(_label_from_xml(node)), _record(env, label)
		{
			_record.wakeup_sigh(wakeup_sigh);
		}

		virtual ~Channel() { };

		void update(Xml_node const &node, Attr const defaults)
		{
			_attr = Attr::from_xml(node, defaults);
		}

		void generate(Xml_generator &xml) const
		{
			unsigned const num_values = min(_capture.count(), 1000u);
			for (unsigned i = 0; i < num_values; i++)
				xml.append_content(_capture.past_value(num_values - i), "\n");
		}

		Record::Num_samples num_samples(unsigned period_ms) const
		{
			return { unsigned((_attr.sample_rate_hz*period_ms)/1000) };
		}

		struct Capture_result
		{
			Record::Time_window tw;
			bool depleted;
		};

		Capture_result capture(Record::Num_samples const num_samples)
		{
			Capture_result result { };
			_record.record(num_samples,
				[&] (Record::Time_window tw, Samples_ptr const &samples) {
					result = { .tw = tw, .depleted = false };
					_capture.insert(samples);
				},
				[&] { /* audio data depleted */
					result = { .tw = { }, .depleted = true };
					for (unsigned i = 0; i < num_samples.value(); i++)
						_capture.insert(0.0f);
				}
			);
			return result;
		}

		void capture_at(Record::Time_window tw,
		                Record::Num_samples num_samples)
		{
			_record.record_at(tw, num_samples,
				[&] (Samples_ptr const &samples) {
					_capture.insert(samples); });
		}

		/*
		 * List_model::Element
		 */
		static bool type_matches(Xml_node const &node)
		{
			return node.has_type("record");
		}

		bool matches(Xml_node const &node) const
		{
			return _label_from_xml(node) == label;
		}
	};

	List_model<Registered<Channel>> _channels { };

	Registry<Registered<Channel>> _channel_registry { }; /* for non-const for_each */

	void _handle_wakeup()
	{
		_timer.trigger_periodic(1000*_period_ms);
	}

	void _handle_config()
	{
		_config.update();

		Xml_node const config = _config.xml();

		_period_ms = config.attribute_value("period_ms",  20u);

		/* channel defaults obtained from top-level config node */
		Channel::Attr const channel_defaults = Channel::Attr::from_xml(config, {
			.sample_rate_hz = 44100u, });

		_channels.update_from_xml(config,
			[&] (Xml_node const &node) -> Registered<Channel> & {
				return *new (_heap)
					Registered<Channel>(_channel_registry, _env, node,
					                    _wakeup_handler); },
			[&] (Registered<Channel> &channel) {
				destroy(_heap, &channel); },
			[&] (Channel &channel, Xml_node const &node) {
				channel.update(node, channel_defaults); }
		);

		_handle_wakeup();
	}

	void _capture_channels()
	{
		/*
		 * The first channel drives the time window that is reused for all
		 * other channels to attain time-synchronized data.
		 */
		bool first = true;
		Channel::Capture_result capture { };
		_channel_registry.for_each([&] (Registered<Channel> &channel) {
			if (first)
				capture = channel.capture(channel.num_samples(_period_ms));
			else
				channel.capture_at(capture.tw, channel.num_samples(_period_ms));
			first = false;
		});

		if (capture.depleted)
			_timer.trigger_periodic(0);
	}

	/**
	 * Dynamic_rom_session::Xml_producer
	 */
	void produce_xml(Xml_generator &xml) override
	{
		_channels.for_each([&] (Channel const &channel) {
			xml.node("channel", [&] {
				xml.attribute("label",   channel.label);
				xml.attribute("rate_hz", channel._attr.sample_rate_hz);
				channel.generate(xml);
			});
		});
	}

	void _handle_timer()
	{
		_capture_channels();
	}

	struct Rom_root : Root_component<Dynamic_rom_session>
	{
		Env  &_env;
		Main &_main;

		Dynamic_rom_session *_create_session(const char *) override
		{
			using namespace Genode;

			return new (md_alloc())
				Dynamic_rom_session(_env.ep(), _env.ram(), _env.rm(), _main);
		}

		Rom_root(Env &env, Allocator &md_alloc, Main &main)
		:
			Root_component<Dynamic_rom_session>(&env.ep().rpc_ep(), &md_alloc),
			_env(env), _main(main)
		{ }

	} _rom_root { _env, _heap, *this };

	Main(Env &env) : Xml_producer("recording"), _env(env)
	{
		_timer.sigh(_timer_handler);
		_config.sigh(_config_handler);
		_handle_config();

		env.parent().announce(_env.ep().manage(_rom_root));
	}
};


void Component::construct(Genode::Env &env)
{
	static Record_rom::Main main(env);
}
