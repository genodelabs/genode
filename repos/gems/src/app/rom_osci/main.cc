/*
 * \brief  Oscilloscope showing data obtained from a dynamic ROM
 * \author Norman Feske
 * \date   2023-12-20
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
#include <gui_session/connection.h>
#include <timer_session/connection.h>
#include <input/event.h>
#include <os/pixel_rgb888.h>
#include <polygon_gfx/line_painter.h>
#include <gems/gui_buffer.h>

namespace Osci {

	using namespace Genode;

	struct Main;

	static void with_skipped_bytes(Const_byte_range_ptr const &bytes,
	                               size_t const n, auto const &fn)
	{
		if (bytes.num_bytes < n)
			return;

		Const_byte_range_ptr const remainder { bytes.start     + n,
		                                       bytes.num_bytes - n };
		fn(remainder);
	}

	static void with_skipped_whitespace(Const_byte_range_ptr const &bytes, auto const &fn)
	{
		auto whitespace = [] (char const c) { return c == ' ' || c == '\n'; };

		size_t skip = 0;
		while (whitespace(bytes.start[skip]) && (skip < bytes.num_bytes))
			skip++;

		Const_byte_range_ptr const remainder { bytes.start     + skip,
		                                       bytes.num_bytes - skip };
		fn(remainder);
	}

	template <typename T>
	static void with_parsed_value(Const_byte_range_ptr const &bytes, auto const &fn)
	{
		T value { };
		size_t const n = ascii_to(bytes.start, value);
		with_skipped_bytes(bytes, n, [&] (Const_byte_range_ptr const &remainder) {
			with_skipped_whitespace(remainder, [&] (Const_byte_range_ptr const &remainder) {
				fn(value, remainder); }); });
	}
}


struct Osci::Main
{
	Env &_env;

	using Point = Gui_buffer::Point;
	using Area  = Gui_buffer::Area;
	using Rect  = Gui_buffer::Rect;

	Area     _size       { };
	Color    _background { };
	unsigned _fps        { };
	bool     _phase_lock { };

	Heap _heap { _env.ram(), _env.rm() };

	Timer::Connection _timer { _env };
	Gui::Connection   _gui   { _env };

	Constructible<Gui_buffer> _gui_buffer { };

	Constructible<Gui::Top_level_view> _view { };

	Attached_rom_dataspace _config    { _env, "config" };
	Attached_rom_dataspace _recording { _env, "recording" };

	Signal_handler<Main> _timer_handler  { _env.ep(), *this, &Main::_handle_timer };
	Signal_handler<Main> _config_handler { _env.ep(), *this, &Main::_handle_config };

	struct Phase_lock { unsigned offset; };

	struct Captured_channel
	{
		static constexpr unsigned SIZE_LOG2 = 10, SIZE = 1 << SIZE_LOG2, MASK = SIZE - 1;

		float _samples[SIZE] { };

		unsigned _pos = 0;

		Captured_channel() { };

		Captured_channel(Xml_node const &channel)
		{
			auto insert = [&] (float value)
			{
				_pos = (_pos + 1) & MASK;
				_samples[_pos] = value;
			};

			channel.with_raw_content([&] (char const *start, size_t len) {
				while (len > 0) {
					Const_byte_range_ptr bytes { start, len };
					with_parsed_value<double>(bytes, [&] (double v, Const_byte_range_ptr const &remaining) {
						insert(float(v));
						start = remaining.start;
						len   = remaining.num_bytes;
					});
				}
			});
		}

		float past_value(unsigned past) const
		{
			return _samples[(_pos - past) & MASK];
		}
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
			double   v_pos, v_scale;
			Color    color;

			static Attr from_xml(Xml_node const &node, Attr const defaults)
			{
				return Attr {
					.v_pos   = node.attribute_value("v_pos",   defaults.v_pos),
					.v_scale = node.attribute_value("v_scale", defaults.v_scale),
					.color   = node.attribute_value("color",   defaults.color),
				};
			}
		} _attr { };

		Captured_channel _capture { };

		Line_painter const _line_painter { };

		Channel(Xml_node const &node) : label(_label_from_xml(node)) { }

		virtual ~Channel() { };

		void update(Xml_node const &node, Attr const defaults)
		{
			_attr = Attr::from_xml(node, defaults);
		}

		void capture(Xml_node const &node) { _capture = Captured_channel(node); }

		Phase_lock phase_lock(unsigned start, float const threshold, unsigned const max) const
		{
			float curr_value = 0.0f;
			for (unsigned i = 0 ; i < max; i++) {
				float const prev_value = curr_value;
				curr_value = _capture.past_value(i + start);
				if (prev_value <= threshold && curr_value > threshold)
					return { i };
			}
			return { 0 };
		}

		void render(Gui_buffer::Pixel_surface &pixel,
		            Gui_buffer::Alpha_surface &, Phase_lock const phase_lock) const
		{
			/*
			 * Draw captured samples from right to left.
			 */

			Area     const area  = pixel.size();
			unsigned const w     = area.w;
			int      const y_pos = int(_attr.v_pos*area.h);
			double   const screen_v_scale = _attr.v_scale*area.h/2;

			auto _horizontal_line = [&] (Color c, int y, Color::channel_t alpha)
			{
				_line_painter.paint(pixel, Point { 0, y },
				                           Point { int(w) - 2, y },
				                    Color { c.r, c.g, c.b, alpha });
			};

			_horizontal_line(_attr.color, y_pos, 80);
			_horizontal_line(_attr.color, y_pos - int(screen_v_scale), 40);
			_horizontal_line(_attr.color, y_pos + int(screen_v_scale), 40);

			Point const centered { 0, y_pos };
			Point previous_p { };
			bool first_iteration = true;
			for (unsigned i = 0; i < w; i++) {

				Point p { int(w - i),
				          int(screen_v_scale*_capture.past_value(i + phase_lock.offset)) };

				p = p + centered;

				if (!first_iteration)
					_line_painter.paint(pixel, p, previous_p, _attr.color);

				previous_p = p;
				first_iteration = false;
			}
		}

		/*
		 * List_model::Element
		 */
		static bool type_matches(Xml_node const &node)
		{
			return node.has_type("channel");
		}

		bool matches(Xml_node const &node) const
		{
			return _label_from_xml(node) == label;
		}
	};

	List_model<Registered<Channel>> _channels { };

	Registry<Registered<Channel>> _channel_registry { }; /* for non-const for_each */

	void _handle_config()
	{
		_config.update();

		Xml_node const config = _config.xml();

		_size       = Area::from_xml(config);
		_background = config.attribute_value("background", Color::black());
		_fps        = config.attribute_value("fps",        50u);
		_phase_lock = config.attribute_value("phase_lock", false);

		_fps = max(_fps, 1u);

		/* channel defaults obtained from top-level config node */
		Channel::Attr const channel_defaults = Channel::Attr::from_xml(config, {
			.v_pos   = 0.5,
			.v_scale = 0.6,
			.color   = Color::rgb(255, 255, 255),
		});

		_gui_buffer.construct(_gui, _size, _env.ram(), _env.rm(),
		                      Gui_buffer::Alpha::OPAQUE, _background);

		_view.construct(_gui, Rect { Point::from_xml(config), _size });

		_channels.update_from_xml(config,
			[&] (Xml_node const &node) -> Registered<Channel> & {
				return *new (_heap) Registered<Channel>(_channel_registry, node); },
			[&] (Registered<Channel> &channel) {
				destroy(_heap, &channel); },
			[&] (Channel &channel, Xml_node const &node) {
				channel.update(node, channel_defaults); }
		);

		_timer.trigger_periodic(1000*1000/_fps);
	}

	void _handle_timer()
	{
		/* import recorded samples */
		_recording.update();
		_recording.xml().for_each_sub_node("channel", [&] (Xml_node const &node) {
			Channel::Label const label = node.attribute_value("label", Channel::Label());
			_channel_registry.for_each([&] (Registered<Channel> &channel) {
				if (channel.label == label)
					channel.capture(node); }); });

		/* determine phase-locking offset from one channel */
		Phase_lock phase_lock { };
		if (_phase_lock)
			_channels.with_first([&] (Channel const &channel) {
				phase_lock = channel.phase_lock(_size.w/2, -0.1f, _size.w/2); });

		_gui_buffer->reset_surface();
		_gui_buffer->apply_to_surface([&] (auto &pixel, auto &alpha) {
			_channels.for_each([&] (Channel const &channel) {
				channel.render(pixel, alpha, phase_lock); }); });

		_gui_buffer->flush_surface();
		_gui.framebuffer.refresh(0, 0, _size.w, _size.h);
	}

	Main(Env &env) : _env(env)
	{
		_timer.sigh(_timer_handler);
		_config.sigh(_config_handler);
		_handle_config();
	}
};


void Component::construct(Genode::Env &env)
{
	static Osci::Main main(env);
}
