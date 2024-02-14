/*
 * \brief  Types used by the mixer
 * \author Norman Feske
 * \date   2023-12-13
 */

/*
 * Copyright (C) 2023 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _TYPES_H_
#define _TYPES_H_

/* Genode includes */
#include <util/string.h>
#include <util/list_model.h>
#include <base/session_label.h>
#include <base/attached_ram_dataspace.h>
#include <record_session/record_session.h>

namespace Mixer {

	using namespace Genode;

	/**
	 * Circular clock in microseconds, wrapping after 4 seconds
	 */
	class Clock
	{
		private:

			unsigned _us = 0;

			static constexpr unsigned LIMIT = (1 << 22),
			                          MASK  = LIMIT - 1;

			static constexpr unsigned _masked(unsigned v) { return v & MASK; }

			static constexpr bool _positive(unsigned v)
			{
				return (v > 0) && (v < LIMIT/2);
			}

		public:

			explicit Clock(unsigned us) : _us(_masked(us)) { };

			Clock() { }

			unsigned us()                 const { return _masked(_us); };
			unsigned us_since(Clock past) const { return _masked(_us - past._us); }

			Clock after_us (unsigned us) const { return Clock { _masked(_us + us) }; }
			Clock before_us(unsigned us) const { return Clock { _masked(_us - us) }; }

			bool earlier_than(Clock const &other) const
			{
				return (other.us() < us()) ? _positive(other.us() + LIMIT - us())
				                           : _positive(other.us() - us());
			}

			bool later_than(Clock const &other) const
			{
				return other.earlier_than(*this);
			}

			static bool range_valid(Clock const &start, Clock const &end)
			{
				return _positive(end.us_since(start));
			}
	};

	struct Clock_operations : Interface
	{
		virtual Clock current_clock_value() = 0;

		/**
		 * Return true if the time is right for latent diagnostic output
		 *
		 * Used for limiting the rate of log messages on account of wrong
		 * audio parameters.
		 */
		virtual bool once_in_a_while() = 0;
	};

	struct Float_range_ptr : Noncopyable
	{
		struct {
			float *start;
			unsigned num_floats;
		};

		Float_range_ptr(float *start, unsigned num_floats)
		: start(start), num_floats(num_floats) { }

		void clear()
		{
			for (size_t i = 0; i < num_floats; i++)
				start[i] = 0.0f;
		}

		void add(Float_range_ptr const &other)
		{
			size_t const limit = min(num_floats, other.num_floats);
			for (unsigned i = 0; i < limit; i++)
				start[i] += other.start[i];
		}

		void scale(float const factor)
		{
			for (size_t i = 0; i < num_floats; i++)
				start[i] *= factor;
		}
	};

	template <unsigned N>
	struct Sample_buffer
	{
		float values[N] { };
		static constexpr unsigned CAPACITY = N;
	};

	using Time_window = Record::Time_window;

	struct Volume
	{
		float value;

		static Volume from_xml(Xml_node const &node)
		{
			return { float(node.attribute_value("volume", 1.0)) };
		}
	};

	struct Sample_producer : Noncopyable, Interface
	{
		virtual bool produce_sample_data(Time_window, Float_range_ptr &) = 0;
	};

	class Play_session;
	using Play_sessions = Registry<Play_session>;

	/**
	 * Call 'fn' for each sub window of a maximum of N samples
	 */
	template <unsigned N>
	void for_each_sub_window(Time_window const tw, Float_range_ptr &samples, auto const &fn)
	{
		if (samples.num_floats == 0)
			return;

		Clock const start { tw.start },
		            end   { tw.end   };

		unsigned pos = 0; /* position within 'samples' */

		auto remain = [&] { return samples.num_floats - pos; };

		/* time per sample in 1/1024 microseconds */
		uint32_t const ascent { (end.us_since(start) << 10) / samples.num_floats };

		while (remain() > 0) {

			/* samples to process in this iteration */
			unsigned const num_samples = min(remain(), N);

			/* window of 'samples' buffer to process in this iteration */
			Float_range_ptr sub_samples(samples.start + pos, num_samples);

			/* sub window of 'tw' that corresponds to the current iteration */
			unsigned const start_offset_us =  pos                * ascent,
			               end_offset_us   = (pos + num_samples) * ascent;

			Time_window const sub_window(start.after_us(start_offset_us >> 10).us(),
			                             start.after_us(end_offset_us   >> 10).us());

			fn(sub_window, sub_samples);

			pos += num_samples;
		}
	}

	unsigned us_from_ms_attr(Xml_node const &node, auto const &attr, double default_value)
	{
		return unsigned(1000*node.attribute_value(attr, default_value));
	}
}


namespace Genode {

	void print(Output &out, Mixer::Time_window const &window)
	{
		using namespace Mixer;
		print(out, window.start/1000, "...", window.end/1000,
		      " (", Clock{window.end}.us_since(Clock{window.start})/1000, ")");
	}
}


static bool operator < (Play::Seq const &l, Play::Seq const &r)
{
	unsigned const LIMIT = Play::Seq::LIMIT;

	auto pos = [] (unsigned v) { return (v > 0) && (v < LIMIT/2); };

	return (r.value() < l.value()) ? pos(r.value() + LIMIT - l.value())
	                               : pos(r.value()         - l.value());
}

#endif /* _TYPES_H_ */
