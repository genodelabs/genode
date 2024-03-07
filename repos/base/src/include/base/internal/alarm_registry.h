/*
 * \brief  Registry of time-sorted alarms
 * \author Norman Feske
 * \date   2024-03-06
 */

/*
 * Copyright (C) 2024 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__BASE__INTERNAL__ALARM_REGISTRY_H_
#define _INCLUDE__BASE__INTERNAL__ALARM_REGISTRY_H_

/* Genode includes */
#include <base/log.h>

namespace Genode { template <typename, typename> class Alarm_registry; }


/**
 * Registry of schedules alarm objects
 *
 * \param T       alarm type, must be derived from 'Alarm_registry::Element'
 * \param CLOCK   type representing a circular clock
 *
 * The registry represents a set of scheduled alarms. An alarm object is
 * scheduled upon creation and de-scheduled on destruction.
 *
 * The 'CLOCK' type must be constructible with an '<unsigned>' numeric value
 * where '<unsigned>' can be an unsigned integer of any byte width.
 * The clock provides the following interface:
 *
 * ! static constexpr <unsigned> MASK = <lsb-bitmask>;
 * ! <unsigned> value() const;
 * ! void print(Output &) const;
 *
 * 'MASK' defines the limit of the circular clock.
 * The 'value()' method returns a number between 0 and MASK.
 * The 'print' method is needed only when using 'Alarm_registry::print'.
 * In this case, the alarm type must also provide a 'print' method.
 */
template <typename T, typename CLOCK>
class Genode::Alarm_registry : Noncopyable
{
	private:

		using Clock = CLOCK;

		struct Range
		{
			Clock start, end; /* range [start,end] where 'start' >= 'end' */

			void with_intersection(Range const other, auto const &fn) const
			{
				auto const f = max(start.value(), other.start.value());
				auto const t = min(end.value(),   other.end.value());

				if (f <= t)
					fn(Range { Clock { f }, Clock { t } });
			}

			bool contains(Clock const time) const
			{
				return (time.value() >= start.value())
				    && (time.value() <= end.value());
			}

			void print(Output &out) const
			{
				Genode::print(out, "[", start.value(), "...", end.value(), "]");
			}
		};

	public:

		struct None { };
		using Soonest_result = Attempt<Clock, None>;

		class Element : Avl_node<Element>
		{
			private:

				Alarm_registry &_registry;

				T &_obj;

				friend class Alarm_registry;
				friend class Avl_node<Element>;
				friend class Avl_tree<Element>;

			public:

				Clock time;

				Element(Alarm_registry &registry, T &obj, Clock time)
				:
					_registry(registry), _obj(obj), time(time)
				{
					_registry._elements.insert(this);
				}

				~Element()
				{
					_registry._elements.remove(this);
				}

				/**
				 * Avl_node ordering, allow duplicated keys
				 */
				bool higher(Element const * const other) const
				{
					return time.value() <= other->time.value();
				}

				void print(Output &out) const
				{
					Genode::print(out, _obj, ": time=", time);
				}

			private:

				static void _with_child(auto &element, bool side, auto const &fn)
				{
					if (element.child(side))
						fn(*element.child(side));
				}

				void _for_each(Range const range, auto const &fn) const
				{
					_with_child(*this, this->LEFT, [&] (Element const &child) {
						range.with_intersection({ Clock { }, time }, [&] (Range l_range) {
							child._for_each(l_range, fn); }); });

					if (range.contains(time))
						fn(_obj);

					_with_child(*this, this->RIGHT, [&] (Element const &child) {
						range.with_intersection({ time, Clock { Clock::MASK }}, [&] (Range r_range) {
							child._for_each(r_range, fn); }); });
				}

				Element *_find_any(Range const range)
				{
					if (range.contains(time))
						return this;

					Element *result = nullptr;

					_with_child(*this, this->LEFT, [&] (Element &child) {
						range.with_intersection({ Clock { }, time }, [&] (Range l_range) {
							result = child._find_any(l_range); }); });

					if (result)
						return result;

					_with_child(*this, this->RIGHT, [&] (Element &child) {
						range.with_intersection({ time, Clock { Clock::MASK }}, [&] (Range r_range) {
							result = child._find_any(r_range); }); });

					return result;
				}

				Soonest_result _soonest(Clock const now) const
				{
					Soonest_result result = None { };

					if (time.value() >= now.value()) {
						result = time;
						_with_child(*this, this->LEFT, [&] (Element const &child) {
							child._soonest(now).with_result(
								[&] (Clock left_soonest) {
									if (time.value() > left_soonest.value())
										result = left_soonest; },
								[&] (None) { }); });
					} else {
						_with_child(*this, this->RIGHT, [&] (Element const &child) {
							result = child._soonest(now); });
					}

					return result;
				}
		};

	private:

		Avl_tree<Element> _elements { };

		static void _with_first(auto &registry, auto const &fn)
		{
			if (registry._elements.first())
				fn(*registry._elements.first());
		}

		/*
		 * Call 'fn' up to two times, with the first element and a search range
		 * as argument.
		 */
		static void _for_each_search_range(auto &registry, Clock start, Clock end,
		                                   auto const &fn)
		{
			_with_first(registry, [&] (auto &first) {

				if (start.value() <= end.value()) {
					fn(first, Range { start, end });

				} else if (start.value() > end.value()) {
					fn(first, Range { start, Clock { Clock::MASK } });
					fn(first, Range { Clock { }, end });
				}
			});
		}

	public:

		/**
		 * Return soonest alarm time from 'now'
		 */
		Soonest_result soonest(Clock const now) const
		{
			Soonest_result result = None { };

			_with_first(*this, [&] (auto &first) {
				first._soonest(now).with_result(
					[&] (Clock soonest) {
						result = soonest; },
					[&] (None) { /* clock wrapped, search from beginning */
						result = first._soonest(Clock { }); }); });

			return result;
		}

		/**
		 * Call 'fn' for each alarm scheduled between 'start' and 'end'
		 *
		 * The 'start' and 'end' values may wrap.
		 */
		void for_each_in_range(Clock const start, Clock const end, auto const &fn) const
		{
			_for_each_search_range(*this, start, end, [&] (Element const &e, Range range) {
				e._for_each(range, fn); });
		}

		/**
		 * Call 'fn' with any alarm scheduled between 'start' and 'end'
		 *
		 * \return true if 'fn' was called
		 *
		 * The found alarm is passed to 'fn' as non-const reference, which
		 * allows the caller to modify or destroy it.
		 *
		 * The return value is handy for calling 'with_any_in_range' as a
		 * condition of a while loop, purging all alarms within the time
		 * window.
		 */
		bool with_any_in_range(Clock const start, Clock const end, auto const &fn)
		{
			Element *found_ptr = nullptr;
			_for_each_search_range(*this, start, end, [&] (Element &e, Range range) {
				if (!found_ptr)
					found_ptr = e._find_any(range); });

			if (found_ptr)
				fn(found_ptr->_obj);

			return found_ptr != nullptr;
		}

		void print(Output &out) const
		{
			bool first = true;
			for_each_in_range(Clock { }, Clock { Clock::MASK }, [&] (Element const &e) {
				Genode::print(out, first ? "" : "\n", e);
				first = false;
			});
		}
};

#endif /* _INCLUDE__BASE__INTERNAL__ALARM_REGISTRY_H_ */
