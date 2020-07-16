/*
 * \brief  Input-event source that accelerates relative motion events
 * \author Norman Feske
 * \date   2017-11-03
 */

/*
 * Copyright (C) 2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _EVENT_FILTER__ACCELERATE_SOURCE_H_
#define _EVENT_FILTER__ACCELERATE_SOURCE_H_

/* Genode includes */
#include <util/bezier.h>
#include <input/keycodes.h>

/* local includes */
#include <source.h>
#include <key_code_by_name.h>

namespace Event_filter { class Accelerate_source; }


class Event_filter::Accelerate_source : public Source, Source::Filter
{
	private:

		Owner _owner;

		Source &_source;

		/**
		 * Look-up table used for the non-linear acceleration of motion values
		 */
		struct Lut
		{
			unsigned char values[256];

			Lut(long curve)
			{
				/* clamp parameter to valid range */
				curve = min(255, max(0, curve));

				auto fill_segment = [&] (long x1, long y1, long x2, long /* y2 */)
				{
					for (long i = x1 >> 8; i <= (x2 >> 8); i++) values[i] = y1 >> 8;
				};

				long const x0 = 0,           y0 = 0,   x1 = curve, y1 = 0,
				           x2 = 255 - curve, y2 = 255, x3 = 255,   y3 = 255;

				bezier(x0<<8, y0<<8, x1<<8, y1<<8, x2<<8, y2<<8, x3<<8, y3<<8,
				       fill_segment, 8);
			}
		};

		Lut const _lut;

		/**
		 * Scale factor applied to incoming motion values before they are
		 * used as index into the LUT.
		 */
		long const _sensitivity_percent;

		/**
		 * Scale factor of values obtained from the LUT. It corresponds to the
		 * maximum increase of motion values.
		 */
		long const _max;

		int _apply_acceleration(int v) const
		{
			int const sign  = (v < 0) ? -1 : 1,
			          index = max(0, min(255, (sign*v*_sensitivity_percent)/100)),
			          accel = (_lut.values[index]*_max)/256;

			return v + sign*accel;
		}

		/**
		 * Filter interface
		 */
		void filter_event(Sink &destination, Input::Event const &event) override
		{
			Input::Event ev = event;

			ev.handle_relative_motion([&] (int x, int y) {
				ev = Input::Relative_motion{_apply_acceleration(x),
				                            _apply_acceleration(y)}; });
			destination.submit(ev);
		}

	public:

		static char const *name() { return "accelerate"; }

		Accelerate_source(Owner &owner, Xml_node config, Source::Factory &factory)
		:
			Source(owner),
			_owner(factory),
			_source(factory.create_source(_owner, input_sub_node(config))),
			_lut                (config.attribute_value("curve", 127L)),
			_sensitivity_percent(config.attribute_value("sensitivity_percent", 100L)),
			_max                (config.attribute_value("max", 20L))
		{ }

		void generate(Sink &destination) override
		{
			Source::Filter::apply(destination, *this, _source);
		}
};

#endif /* _EVENT_FILTER__ACCELERATE_SOURCE_H_ */
