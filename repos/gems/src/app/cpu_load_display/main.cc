/*
 * \brief  CPU load display
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
#include <polygon_gfx/shaded_polygon_painter.h>
#include <polygon_gfx/interpolate_rgb565.h>
#include <os/pixel_alpha8.h>
#include <nano3d/scene.h>
#include <nano3d/sincos_frac16.h>
#include <gems/color_hsv.h>


namespace Cpu_load_display {

	class Timeline;
	class Cpu;
	class Cpu_registry;
	template <typename> class Scene;

	typedef Genode::Xml_node Xml_node;
	typedef Genode::Color    Color;

	using Genode::max;
};


class Cpu_load_display::Timeline : public Genode::List<Timeline>::Element
{
	public:

		enum { HISTORY_LEN = 32 };

		typedef Genode::String<160> Label;

	private:

		unsigned const _subject_id = 0;

		unsigned _activity[HISTORY_LEN];

		unsigned _sum_activity = 0;

		Label _label;

		/**
		 * Return hue value based on subject ID
		 */
		unsigned _hue() const
		{
			/*
			 * To get nicely varying hue values, we pass the subject ID
			 * to a hash function.
			 */
			unsigned int const a = 1588635695, q = 2, r = 1117695901;
			return (a*(_subject_id % q) - r*(_subject_id / q)) & 255;
		}

	public:

		Timeline(unsigned subject_id, Label const &label)
		:
			_subject_id(subject_id), _label(label)
		{
			Genode::memset(_activity, 0, sizeof(_activity));
		}

		void activity(unsigned long recent_activity, unsigned now)
		{
			unsigned const i = now % HISTORY_LEN;

			_sum_activity -= _activity[i];

			_activity[i] = recent_activity;

			_sum_activity += recent_activity;
		}

		unsigned long activity(unsigned i) const
		{
			return _activity[i % HISTORY_LEN];
		}

		bool has_subject_id(unsigned subject_id) const
		{
			return _subject_id == subject_id;
		}

		bool idle() const { return _sum_activity == 0; }

		bool kernel() const
		{
			return _label == Label("kernel");
		}

		enum Color_type { COLOR_TOP, COLOR_BOTTOM };

		Color color(Color_type type) const
		{
			unsigned const brightness = 140;
			unsigned const saturation = type == COLOR_TOP ? 70 : 140;
			unsigned const alpha      = 230;

			Color const c = color_from_hsv(_hue(), saturation, brightness);
			return Color(c.r, c.g, c.b, alpha);
		}
};


class Cpu_load_display::Cpu : public Genode::List<Cpu>::Element
{
	private:

		Genode::Allocator     &_heap;
		Genode::Point<>  const _pos;
		Genode::List<Timeline> _timelines { };

		Timeline *_lookup_timeline(Xml_node subject)
		{
			unsigned long const subject_id = subject.attribute_value("id", 0UL);

			Timeline::Label const label =
				subject.attribute_value("label", Timeline::Label());

			for (Timeline *t = _timelines.first(); t; t = t->next()) {
				if (t->has_subject_id(subject_id))
					return t;
			}

			/* add new timeline */
			Timeline *t = new (_heap) Timeline(subject_id, label);
			_timelines.insert(t);
			return t;
		}

		unsigned long _activity(Xml_node subject)
		{
			try {
				Xml_node activity = subject.sub_node("activity");
				return activity.attribute_value("recent", 0UL);
			} catch (Xml_node::Nonexistent_sub_node) { }

			return 0;
		}

	public:

		Cpu(Genode::Allocator &heap, Genode::Point<> pos)
		: _heap(heap), _pos(pos) { }

		bool has_pos(Genode::Point<> pos) const
		{
			return pos == _pos;
		}

		void import_trace_subject(Xml_node subject, unsigned now)
		{
			unsigned long const activity = _activity(subject);

			if (activity)
				_lookup_timeline(subject)->activity(activity, now);
		}

		void advance(unsigned now)
		{
			Timeline *next = nullptr;
			for (Timeline *t = _timelines.first(); t; t = next) {

				next = t->next();

				t->activity(0, now);

				if (t->idle()) {

					_timelines.remove(t);
					Genode::destroy(_heap, t);
				}
			}
		}

		unsigned long activity_sum(unsigned i) const
		{
			unsigned long sum = 0;

			for (Timeline const *t = _timelines.first(); t; t = t->next())
				sum += t->activity(i);

			return sum;
		}

		template <typename FN>
		void for_each_timeline(FN const &fn) const
		{
			for (Timeline const *t = _timelines.first(); t; t = t->next())
				fn(*t);
		}
};


class Cpu_load_display::Cpu_registry
{
	private:

		Genode::Allocator &_heap;

		Genode::List<Cpu> _cpus { };

		static Genode::Point<> _cpu_pos(Xml_node subject)
		{
			try {
				Xml_node affinity = subject.sub_node("affinity");
				return Genode::Point<>(affinity.attribute_value("xpos", 0UL),
				                       affinity.attribute_value("ypos", 0UL));
			} catch (Xml_node::Nonexistent_sub_node) { }

			return Genode::Point<>(0, 0);
		}

		Cpu *_lookup_cpu(Xml_node subject)
		{
			/* find CPU that matches the affinity of the subject */
			Genode::Point<> cpu_pos = _cpu_pos(subject);
			for (Cpu *cpu = _cpus.first(); cpu; cpu = cpu->next()) {
				if (cpu->has_pos(cpu_pos))
					return cpu;
			}

			/* add new CPU */
			Cpu *cpu = new (_heap) Cpu(_heap, cpu_pos);
			_cpus.insert(cpu);
			return cpu;
		}

		void _import_trace_subject(Xml_node subject, unsigned now)
		{
			Cpu *cpu = _lookup_cpu(subject);

			cpu->import_trace_subject(subject, now);
		}

	public:

		Cpu_registry(Genode::Allocator &heap) : _heap(heap) { }

		void import_trace_subjects(Xml_node node, unsigned now)
		{
			node.for_each_sub_node("subject", [&] (Xml_node subject) {
				_import_trace_subject(subject, now); });
		}

		template <typename FN>
		void for_each_cpu(FN const &fn) const
		{
			for (Cpu const *cpu = _cpus.first(); cpu; cpu = cpu->next())
				fn(*cpu);
		}

		void advance(unsigned now)
		{
			for (Cpu *cpu = _cpus.first(); cpu; cpu = cpu->next())
				cpu->advance(now);
		}
};


template <typename PT>
class Cpu_load_display::Scene : public Nano3d::Scene<PT>
{
	private:

		Genode::Env &_env;

		Nitpicker::Area const _size;

		Genode::Attached_rom_dataspace _config { _env, "config" };

		void _handle_config() { _config.update(); }

		Genode::Signal_handler<Scene> _config_handler;

		Genode::Attached_rom_dataspace _trace_subjects { _env, "trace_subjects" };

		unsigned _now = 0;

		Genode::Heap _heap { _env.ram(), _env.rm() };

		Cpu_registry _cpu_registry { _heap };

		void _handle_trace_subjects()
		{
			_trace_subjects.update();

			if (!_trace_subjects.valid())
				return;

			_cpu_registry.advance(++_now);

			try {
				Xml_node subjects(_trace_subjects.local_addr<char>());
				_cpu_registry.import_trace_subjects(subjects, _now);
			} catch (...) { Genode::error("failed to import trace subjects"); }
		}

		Genode::Signal_handler<Scene> _trace_subjects_handler;

	public:

		Scene(Genode::Env &env, Genode::uint64_t update_rate_ms,
		      Nitpicker::Point pos, Nitpicker::Area size)
		:
			Nano3d::Scene<PT>(env, update_rate_ms, pos, size),
			_env(env), _size(size),
			_config_handler(env.ep(), *this, &Scene::_handle_config),
			_trace_subjects_handler(env.ep(), *this, &Scene::_handle_trace_subjects)
		{
			_config.sigh(_config_handler);
			_handle_config();

			_trace_subjects.sigh(_trace_subjects_handler);
		}

	private:

		Polygon::Shaded_painter _shaded_painter { _heap, _size.h() };

		long _activity_sum[Timeline::HISTORY_LEN];
		long _y_level[Timeline::HISTORY_LEN];
		long _y_curr[Timeline::HISTORY_LEN];

		void _plot_cpu(Genode::Surface<PT>                   &pixel,
		               Genode::Surface<Genode::Pixel_alpha8> &alpha,
		               Cpu const &cpu, Nitpicker::Rect rect)
		{
			enum { HISTORY_LEN = Timeline::HISTORY_LEN };

			/* calculate activity sum for each point in history */
			for (unsigned i = 0; i < HISTORY_LEN; i++)
				_activity_sum[i] = cpu.activity_sum(i);

			for (unsigned i = 0; i < HISTORY_LEN; i++)
				_y_level[i] = 0;

			int const h = rect.h();
			int const w = rect.w();

			cpu.for_each_timeline([&] (Timeline const &timeline) {

				if (timeline.kernel())
					return;

				bool first = true;

				/* reset values of the current timeline */
				for (unsigned i = 0; i < HISTORY_LEN; i++)
					_y_curr[i] = 0;

				Color const top_color    = timeline.color(Timeline::COLOR_TOP);
				Color const bottom_color = timeline.color(Timeline::COLOR_BOTTOM);

				for (unsigned i = 0; i < HISTORY_LEN; i++) {

					unsigned const t      = (_now - i - 0) % HISTORY_LEN;
					unsigned const prev_t = (_now - i + 1) % HISTORY_LEN;

					unsigned long const activity = timeline.activity(t);

					int const dy = _activity_sum[t] ? (activity*h) / _activity_sum[t] : 0;

					_y_curr[t] = _y_level[t] + dy;

					if (!first) {

						/* draw polygon */
						int const n  = HISTORY_LEN - 1;
						int const x0 = ((n - i + 0)*w)/n + rect.x1();
						int const x1 = ((n - i + 1)*w)/n + rect.x1();

						int const y0 = rect.y1() + h - _y_curr[t];
						int const y1 = rect.y1() + h - _y_curr[prev_t];
						int const y2 = rect.y1() + h - _y_level[prev_t];
						int const y3 = rect.y1() + h - _y_level[t];

						typedef Polygon::Shaded_painter::Point Point;
						Point points[4];
						points[0] = Point(x0, y0, top_color);
						points[1] = Point(x1, y1, top_color);
						points[2] = Point(x1, y2, y1 == y2 ? top_color : bottom_color);
						points[3] = Point(x0, y3, y3 == y0 ? top_color : bottom_color);
						_shaded_painter.paint(pixel, alpha, points, 4);

						/* drop shadow */
						Color const black       (0, 0, 0, 100);
						Color const translucent (0, 0, 0,   0);

						points[0] = Point(x0, y3 - 5, translucent);
						points[1] = Point(x1, y2 - 5, translucent);
						points[2] = Point(x1, y2, black);
						points[3] = Point(x0, y3, black);

						_shaded_painter.paint(pixel, alpha, points, 4);
					}
					first = false;
				}

				/* raise level by the values of the current timeline */
				for (unsigned i = 0; i < HISTORY_LEN; i++)
					_y_level[i] = _y_curr[i];

			});
		}

	public:

		/**
		 * Scene interface
		 */
		void render(Genode::Surface<PT>                   &pixel,
		            Genode::Surface<Genode::Pixel_alpha8> &alpha) override
		{
			/* background */
			Color const top_color    = Color(10, 10, 10, 20);
			Color const bottom_color = Color(10, 10, 10, 100);

			unsigned const w = pixel.size().w();
			unsigned const h = pixel.size().h();

			typedef Polygon::Shaded_painter::Point Point;
			Point points[4];
			points[0] = Point(0,     0,     top_color);
			points[1] = Point(w - 1, 0,     top_color);
			points[2] = Point(w - 1, h - 1, bottom_color);
			points[3] = Point(0,     h - 1, bottom_color);
			_shaded_painter.paint(pixel, alpha, points, 4);

			/* determine number of CPUs */
			unsigned num_cpus = 0;
			_cpu_registry.for_each_cpu([&] (Cpu const &) { num_cpus++; });

			if (num_cpus == 0)
				return;

			/* plot graphs for the CPUs below each other */
			enum { GAP = 8 };
			Nitpicker::Point const step(0, _size.h()/num_cpus);
			Nitpicker::Area  const size(_size.w(), step.y() - GAP);
			Nitpicker::Point       point(0, GAP/2);

			_cpu_registry.for_each_cpu([&] (Cpu const &cpu) {
				_plot_cpu(pixel, alpha, cpu, Nitpicker::Rect(point, size));
				point = point + step;
			});
		}
};


void Component::construct(Genode::Env &env)
{
	enum { UPDATE_RATE_MS = 250 };

	static Cpu_load_display::Scene<Genode::Pixel_rgb565>
		scene(env, UPDATE_RATE_MS,
		      Nitpicker::Point(0, 0), Nitpicker::Area(400, 400));
}
