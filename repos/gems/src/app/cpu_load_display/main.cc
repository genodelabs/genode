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
#include <os/pixel_rgb888.h>
#include <os/pixel_alpha8.h>
#include <nano3d/scene.h>
#include <nano3d/sincos_frac16.h>
#include <gems/color_hsv.h>


namespace Cpu_load_display {

	using namespace Genode;

	class Timeline;
	class Cpu;
	class Cpu_registry;
	template <typename> class Scene;

	struct Subject_id { unsigned value; };
	struct Activity   { uint64_t value; };
};


class Cpu_load_display::Timeline : public List<Timeline>::Element
{
	public:

		static constexpr unsigned HISTORY_LEN = 32;

		using Label = String<160>;

		struct Index { unsigned value; };

	private:

		Subject_id const _subject_id;

		Activity _activity[HISTORY_LEN] { };

		Activity _sum_activity { };

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
			return (a*(_subject_id.value % q) - r*(_subject_id.value / q)) & 255;
		}

	public:

		Timeline(Subject_id subject_id, Label const &label)
		:
			_subject_id(subject_id), _label(label)
		{ }

		void activity(Activity recent_activity, unsigned now)
		{
			unsigned const i = now % HISTORY_LEN;

			_sum_activity.value -= _activity[i].value;

			_activity[i] = recent_activity;

			_sum_activity.value += recent_activity.value;
		}

		Activity activity(Index const i) const
		{
			return _activity[i.value % HISTORY_LEN];
		}

		bool has_subject_id(Subject_id const subject_id) const
		{
			return _subject_id.value == subject_id.value;
		}

		bool idle() const { return _sum_activity.value == 0; }

		bool kernel() const { return _label == Label("kernel"); }

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


class Cpu_load_display::Cpu : public List<Cpu>::Element
{
	public:

		using Affinity = Point<>;

	private:

		Allocator     &_heap;
		Affinity const _pos;
		List<Timeline> _timelines { };

		void _with_timeline(Xml_node const &subject, auto const &fn)
		{
			Subject_id const subject_id { subject.attribute_value("id", 0U) };

			Timeline::Label const label =
				subject.attribute_value("label", Timeline::Label());

			for (Timeline *t = _timelines.first(); t; t = t->next()) {
				if (t->has_subject_id(subject_id)) {
					fn(*t);
					return; } }

			/* add new timeline */
			Timeline &t = *new (_heap) Timeline(subject_id, label);
			_timelines.insert(&t);
			fn(t);
		}

		Activity _activity(Xml_node const &subject)
		{
			Activity result { };
			subject.with_optional_sub_node("activity", [&] (Xml_node const &activity) {
				result = { activity.attribute_value("recent", 0U) }; });

			return result;
		}

	public:

		Cpu(Allocator &heap, Affinity pos) : _heap(heap), _pos(pos) { }

		bool has_pos(Affinity const pos) const { return pos == _pos; }

		void import_trace_subject(Xml_node const &subject, unsigned now)
		{
			Activity const activity = _activity(subject);

			if (activity.value)
				_with_timeline(subject, [&] (Timeline &timeline) {
					timeline.activity(activity, now); });
		}

		void advance(unsigned now)
		{
			Timeline *next = nullptr;
			for (Timeline *t = _timelines.first(); t; t = next) {

				next = t->next();

				t->activity({ 0 }, now);

				if (t->idle()) {

					_timelines.remove(t);
					destroy(_heap, t);
				}
			}
		}

		Activity activity_sum(Timeline::Index const i) const
		{
			uint64_t sum = 0;

			for (Timeline const *t = _timelines.first(); t; t = t->next())
				sum += t->activity(i).value;

			return { sum };
		}

		void for_each_timeline(auto const &fn) const
		{
			for (Timeline const *t = _timelines.first(); t; t = t->next())
				fn(*t);
		}
};


class Cpu_load_display::Cpu_registry
{
	private:

		Allocator &_heap;

		List<Cpu> _cpus { };

		static Cpu::Affinity _cpu_pos(Xml_node const &subject)
		{
			Cpu::Affinity result { 0, 0 };
			subject.with_optional_sub_node("affinity", [&] (Xml_node const &affinity) {
				result = Cpu::Affinity::from_xml(affinity); });

			return result;
		}

		void _with_cpu(Xml_node const &subject, auto const &fn)
		{
			/* find CPU that matches the affinity of the subject */
			Cpu::Affinity const cpu_pos = _cpu_pos(subject);
			for (Cpu *cpu = _cpus.first(); cpu; cpu = cpu->next()) {
				if (cpu->has_pos(cpu_pos)) {
					fn(*cpu);
					return; } }

			/* add new CPU */
			Cpu &cpu = *new (_heap) Cpu(_heap, cpu_pos);
			_cpus.insert(&cpu);
			fn(cpu);
		}

		void _import_trace_subject(Xml_node const &subject, unsigned now)
		{
			_with_cpu(subject, [&] (Cpu &cpu) {
				cpu.import_trace_subject(subject, now); });
		}

	public:

		Cpu_registry(Allocator &heap) : _heap(heap) { }

		void import_trace_subjects(Xml_node const &node, unsigned now)
		{
			node.for_each_sub_node("subject", [&] (Xml_node const &subject) {
				_import_trace_subject(subject, now); });
		}

		void for_each_cpu(auto const &fn) const
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

		Env &_env;

		Gui::Area const _size;

		Attached_rom_dataspace _config { _env, "config" };

		void _handle_config() { _config.update(); }

		Signal_handler<Scene> _config_handler;

		Attached_rom_dataspace _trace_subjects { _env, "trace_subjects" };

		unsigned _now = 0;

		Heap _heap { _env.ram(), _env.rm() };

		Cpu_registry _cpu_registry { _heap };

		void _handle_trace_subjects()
		{
			_trace_subjects.update();

			if (!_trace_subjects.valid())
				return;

			_cpu_registry.advance(++_now);

			_cpu_registry.import_trace_subjects(_trace_subjects.xml(), _now);
		}

		Signal_handler<Scene> _trace_subjects_handler;

	public:

		Scene(Env &env, uint64_t update_rate_ms,
		      Gui::Point pos, Gui::Area size)
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

		Polygon::Shaded_painter _shaded_painter { _heap, _size.h };

		static constexpr unsigned HISTORY_LEN = Timeline::HISTORY_LEN;

		Activity _activity_sum [HISTORY_LEN];
		int      _y_level      [HISTORY_LEN];
		int      _y_curr       [HISTORY_LEN];

		void _plot_cpu(Surface<PT> &pixel, Surface<Pixel_alpha8> &alpha,
		               Cpu const &cpu, Gui::Rect rect)
		{
			/* calculate activity sum for each point in history */
			for (unsigned i = 0; i < HISTORY_LEN; i++)
				_activity_sum[i] = cpu.activity_sum(Timeline::Index{i});

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

					uint64_t const activity = timeline.activity(Timeline::Index{t}).value;

					int const dy = _activity_sum[t].value
					             ? int((activity*h) / _activity_sum[t].value)
					             : 0;

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

						using Point = Polygon::Shaded_painter::Point;
						{
							Point const points[4] {
								Point(x0, y0, top_color),
								Point(x1, y1, top_color),
								Point(x1, y2, y1 == y2 ? top_color : bottom_color),
								Point(x0, y3, y3 == y0 ? top_color : bottom_color) };

							_shaded_painter.paint(pixel, alpha, points, 4);
						}

						/* drop shadow */
						{
							Color const black       (0, 0, 0, 100);
							Color const translucent (0, 0, 0,   0);
							Point const points[4] {
								Point(x0, y3 - 5, translucent),
								Point(x1, y2 - 5, translucent),
								Point(x1, y2, black),
								Point(x0, y3, black) };

							_shaded_painter.paint(pixel, alpha, points, 4);
						}
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
		void render(Surface<PT> &pixel, Surface<Pixel_alpha8> &alpha) override
		{
			/* background */
			Color const top_color    { 10, 10, 10, 20  };
			Color const bottom_color { 10, 10, 10, 100 };

			unsigned const w = pixel.size().w;
			unsigned const h = pixel.size().h;

			using Point = Polygon::Shaded_painter::Point;

			Point const points[4] { Point(0,     0,     top_color),
			                        Point(w - 1, 0,     top_color),
			                        Point(w - 1, h - 1, bottom_color),
			                        Point(0,     h - 1, bottom_color) };

			_shaded_painter.paint(pixel, alpha, points, 4);

			/* determine number of CPUs */
			unsigned num_cpus = 0;
			_cpu_registry.for_each_cpu([&] (Cpu const &) { num_cpus++; });

			if (num_cpus == 0)
				return;

			/* plot graphs for the CPUs below each other */
			int const GAP = 8;
			Gui::Point const step(0, _size.h/num_cpus);
			Gui::Area  const size(_size.w, step.y - GAP);
			Gui::Point       point(0, GAP/2);

			_cpu_registry.for_each_cpu([&] (Cpu const &cpu) {
				_plot_cpu(pixel, alpha, cpu, Gui::Rect(point, size));
				point = point + step; });
		}
};


void Component::construct(Genode::Env &env)
{
	Genode::uint64_t const UPDATE_RATE_MS = 250;

	static Cpu_load_display::Scene<Genode::Pixel_rgb888>
		scene(env, UPDATE_RATE_MS,
		      Gui::Point(0, 0), Gui::Area(400, 400));
}
