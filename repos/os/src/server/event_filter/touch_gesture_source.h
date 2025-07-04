/*
 * \brief  Input-event source that generates press/release from touch gestures
 * \author Johannes Schlatow
 * \date   2025-03-19
 *
 * This filter generates artificial key press/release event pairs when touch
 * gestures are detected.
 */

/*
 * Copyright (C) 2025 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _EVENT_FILTER__TOUCH_GESTURE_SOURCE_H_
#define _EVENT_FILTER__TOUCH_GESTURE_SOURCE_H_

/* Genode includes */
#include <input/keycodes.h>
#include <util/geometry.h>
#include <base/allocator.h>
#include <base/duration.h>

/* local includes */
#include <source.h>

namespace Event_filter { class Touch_gesture_source; }


class Event_filter::Touch_gesture_source : public Source, Source::Filter
{
	private:

		using Rect  = Genode::Rect<>;
		using Area  = Genode::Area<>;
		using Point = Genode::Point<>;
		using Microseconds = Genode::Microseconds;

		Owner _owner;

		Source &_source;

		Allocator &_alloc;

		enum State { IDLE, DETECT, TRIGGERED } _state { IDLE };

		/*
		 * Buffer interface used by gestures
		 */
		struct Buffer_action : Interface
		{
			virtual void clear() = 0;
			virtual void submit(Sink &) = 0;
		};

		/*
		 * Event buffer for postponing input events
		 */
		struct Event_buffer : Buffer_action
		{
			enum { MAX_EVENTS = 200 };

			Input::Event _events[MAX_EVENTS];
			unsigned     _count { 0 };

			void store(Input::Event const &e)
			{
				if (_count < MAX_EVENTS)
					_events[_count++] = e;
			}

			/*
			 * Buffer_action interface
			 */

			void clear() override { _count = 0; }

			void submit(Source::Sink &destination) override
			{
				for (unsigned i = 0; i < _count; i++)
					destination.submit(_events[i]);
			}

		} _buffer { };

		/*
		 * Multitouch state tracker
		 */
		struct Multitouch
		{
			enum { MAX_FINGERS = 4 };

			enum Direction { ANY, UP, DOWN, LEFT, RIGHT };

			struct Finger
			{
				Point last_pos;
				int   distance_x   { 0 };
				int   distance_y   { 0 };

				unsigned distance(Direction dir) const
				{
					switch (dir)
					{
						case UP:
							return (unsigned)max(0, -distance_y);
						case DOWN:
							return (unsigned)max(0, distance_y);
						case LEFT:
							return (unsigned)max(0, -distance_x);
						case RIGHT:
							return (unsigned)max(0, distance_x);
						default:
							break;
					}

					auto abs = [] (auto v) { return v >= 0 ? v : -v; };

					/* ANY */
					return (unsigned)max(abs(distance_x), abs(distance_y));
				}

				Direction direction() const
				{
					auto abs = [] (auto v) { return v >= 0 ? v : -v; };
					unsigned const abs_x = abs(distance_x);
					unsigned const abs_y = abs(distance_y);

					if (abs_x > abs_y)
						return distance_x > 0 ? RIGHT : LEFT;
					else if (abs_y > abs_x)
						return distance_y > 0 ? DOWN : UP;

					/* abs_x == abs_y */
					return ANY;
				}

				Finger(Point p) : last_pos(p) { }
			};

			Constructible<Finger> _fingers[MAX_FINGERS];

			unsigned              _present { 0 };

			void handle_event(Input::Event const &ev)
			{
				ev.handle_touch([&] (Input::Touch_id id, float x, float y) {
					if (id.value >= MAX_FINGERS)
						return;

					Constructible<Finger> &finger { _fingers[id.value] };

					Point p { (int)x, (int)y };

					if (!finger.constructed()) {
						finger.construct(p);
						_present++;
					} else {
						Point diff = p - finger->last_pos;

						finger->distance_x   += diff.x;
						finger->distance_y   += diff.y;

						finger->last_pos = p;
					}
				});

				ev.handle_touch_release([&] (Input::Touch_id id) {
					if (id.value >= MAX_FINGERS)
						return;

					if (_fingers[id.value].constructed()) {
						_present--;
						_fingers[id.value].destruct();
					}
				});
			}

			unsigned fingers_present() const { return _present; }

			template <typename FN>
			void for_each(FN && fn) const
			{
				for (unsigned i = 0; i < MAX_FINGERS; i++) {
					if (!_fingers[i].constructed()) continue;

					fn(*_fingers[i]);
				}
			}

		} _multitouch { };

		struct Gesture : Interface, private Registry<Gesture>::Element
		{
			Buffered_node _node;

			State _state { State::IDLE };

			State state() { return _state; }

			/*
			 * - handles touch and touch_release events
			 * - can submit generated events
			 */
			virtual void handle_event(Sink &, Input::Event const &) = 0;

			/*
			 * - called when filter is in state TRIGGERED
			 * - can submit buffer and clear buffer
			 * - can inject generated events
			 */
			virtual void generate(Sink &, Buffer_action &) = 0;
 
			/*
			 * cancel gesture detection
			 */
			virtual void cancel() = 0;

			static void _emit_from_node(Source::Sink &destination,
			                            Node   const &node,
			                            bool          release)
			{
				node.for_each_sub_node("key", [&] (Node const &key) {
					Input::Keycode code { Input::KEY_UNKNOWN };
					try {
						code = key_code_by_name(key.attribute_value("name", Key_name()));
					} catch (Unknown_key) { }

					if (!release)
						destination.submit(Input::Press   { code });

					_emit_from_node(destination, key, release);

					bool const hold = key.attribute_value("hold", false);
					if (release == hold)
						destination.submit(Input::Release { code });
				});
			}

			Gesture(Registry<Gesture> &registry, Allocator &alloc, Node const &node)
			: Registry<Gesture>::Element(registry, *this), _node(alloc, node) { }
		};

		/*
		 * Swipe gesture: detects (multi-)finger swipes
		 * - can be limited to a particular direction (up, down, left, right)
		 * - can be limited to a certain rect
		 * - has a minimum distance after which it will be triggered
		 * - has a maximum time after which gesture detection is cancelled
		 */
		struct Swipe : Gesture
		{
			Timer::Connection &_timer;

			Timer::One_shot_timeout<Swipe> _timeout {
				_timer, *this, &Swipe::_handle_timeout };

			using Direction = Multitouch::Direction;

			struct Attr
			{
				Rect         rect;
				unsigned     distance;
				Direction    direction;
				Microseconds duration;
				unsigned     fingers;

				static Direction _direction_from_node(Node const &node)
				{
					String<8> value { "" };
					value = node.attribute_value("direction", value);

					if (value == "up")    return Direction::UP;
					if (value == "down")  return Direction::DOWN;
					if (value == "left")  return Direction::LEFT;
					if (value == "right") return Direction::RIGHT;

					return Direction::ANY;
				}

				static Microseconds _duration_from_node(Node const &node)
				{
					return Microseconds {
						node.attribute_value("duration_ms", 1000U)*1000 };
				}

				static Attr from_node(Node const &node)
				{
					return {
						.rect      = Rect::from_node(node),
						.distance  = node.attribute_value("distance", 100U),
						.direction = _direction_from_node(node),
						.duration  = _duration_from_node(node),
						.fingers   = node.attribute_value("fingers", 1U)
					};
				}
			};

			Attr const _attr;

			Multitouch const &_multitouch;

			void _handle_timeout(Duration) { cancel(); }

			void cancel() override
			{
				if (_state == IDLE)
					return;

				if (_timeout.scheduled())
					_timeout.discard();

				_state = State::IDLE;
			}

			bool _detected()
			{
				if (_multitouch.fingers_present() != _attr.fingers)
					return false;

				unsigned fingers_okay { 0 };
				_multitouch.for_each([&] (Multitouch::Finger const &finger) {
					if (finger.direction() != _attr.direction)
						return;

					if (finger.distance(_attr.direction) >= _attr.distance)
						fingers_okay++;
				});

				return fingers_okay == _attr.fingers;
			}

			void handle_event(Sink &destination, Input::Event const &ev) override
			{
				ev.handle_touch([&] (Input::Touch_id, float x, float y) {

					Point p {(int)x, (int)y};
					switch (_state)
					{
						case IDLE:
							if (_attr.rect.valid() && !_attr.rect.contains(p))
								return;

							_state = State::DETECT;
							_timeout.schedule(_attr.duration);
							[[fallthrough]];
						case DETECT:
							if (_multitouch.fingers_present() > _attr.fingers) {
								cancel();
								return;
							}

							if (_detected()) {
								_state = State::TRIGGERED;
								_timeout.discard();

								_emit_from_node(destination, _node, false);
							}

							break;
						case TRIGGERED:
							/* nothing to be done */
							break;
					}
				});

				if (_state == IDLE)
					return;

				ev.handle_touch_release([&] (Input::Touch_id) {
					if (_multitouch.fingers_present() == 0) {
						/* emit release events if gesture had been triggered */
						if (_state == TRIGGERED)
							_emit_from_node(destination, _node, true);
						cancel();
					}
				});
			}

			void generate(Source::Sink &, Buffer_action &buffer) override
			{
				if (_state != State::TRIGGERED)
					return;

				buffer.clear();
			}

			Swipe(Registry<Gesture> &registry,
			      Timer::Connection &timer,
			      Allocator         &alloc,
			      Multitouch const  &multitouch,
			      Node       const  &node)
			: Gesture(registry, alloc, node), _timer(timer),
			  _attr(Attr::from_node(node)),
			  _multitouch(multitouch)
			{
				if (_attr.fingers > Multitouch::MAX_FINGERS)
					warning("Swipe gesture limited to ",
					        (unsigned)Multitouch::MAX_FINGERS, " fingers");
			}
		};

		/*
		 * Hold gesture: Triggers if number of fingers is held for a certain time
		 * - The fingers must stay within a certain area around the first touch.
		 * - If the finger is held after the gesture triggered, touch events
		 *   are translated into absolute motion events.
		 */
		struct Hold : Gesture
		{
			Timer::Connection &_timer;
			Source::Trigger   &_trigger;

			Timer::One_shot_timeout<Hold> _timeout {
				_timer, *this, &Hold::_handle_timeout };

			struct Attr
			{
				Area         area;
				Microseconds delay;
				unsigned     fingers;

				static Microseconds _delay_from_node(Node const &node)
				{
					return Microseconds {
						node.attribute_value("delay_ms", 1000U)*1000 };
				}

				static Attr from_node(Node const &node)
				{
					Attr attr {
						.area    = Area::from_node(node),
						.delay   = _delay_from_node(node),
						.fingers = node.attribute_value("fingers", 1U)
					};

					if (attr.area.w == 0) attr.area.w = 30;
					if (attr.area.h == 0) attr.area.h = 30;

					return attr;
				}
			};

			Attr const          _attr;

			Multitouch const   &_multitouch;

			/* Rect of size _attr.area around the starting touch */
			Constructible<Rect> _rect { };
			Point               _start_pos { };
			bool                _emitted { false };

			void _handle_timeout(Duration)
			{
				_state   = State::TRIGGERED;
				_emitted = false;
				_trigger.trigger_generate();
			}

			void cancel() override
			{
				if (_state == State::IDLE)
					return;

				_timeout.discard();
				_state = State::IDLE;
			}

			void handle_event(Sink &destination, Input::Event const &ev) override
			{
				ev.handle_touch([&] (Input::Touch_id id, float x, float y) {

					Point p {(int)x, (int)y};
					Point diff;
					switch (_state)
					{
						case IDLE:
							_rect.construct(
								p - Point { (int)_attr.area.w/2, (int)_attr.area.h/2 },
								_attr.area
							);
							_start_pos = p;
							_state = State::DETECT;
							[[fallthrough]];
						case DETECT:
							if (_multitouch.fingers_present() > _attr.fingers || !_rect->contains(p)) {
								cancel();
								return;
							}
							if (_multitouch.fingers_present() == _attr.fingers && !_timeout.scheduled())
								_timeout.schedule(_attr.delay);
							break;
						case TRIGGERED:
							/* translate into relative motion events */
							if (id.value == 0) {
								diff = p - _start_pos;
								destination.submit(Input::Relative_motion { diff.x, diff.y });
								_start_pos = p;
							}
							break;
					}
				});

				if (_state == IDLE)
					return;

				ev.handle_touch_release([&] (Input::Touch_id) {
					if (_multitouch.fingers_present() == 0) {
						_timeout.discard();

						/* emit release events if gesture had been triggered */
						if (_state == TRIGGERED)
							_emit_from_node(destination, _node, true);

						cancel();
					}
				});
			}

			void generate(Source::Sink &destination, Buffer_action &buffer) override
			{
				if (_state != State::TRIGGERED || _emitted)
					return;

				/* emit absolute motion to trigger focus handling */
				destination.submit(Input::Absolute_motion { _start_pos.x, _start_pos.y });

				_emit_from_node(destination, _node, false);
				buffer.clear();
				_emitted = true;
			}

			Hold(Registry<Gesture> &registry,
			     Timer::Connection &timer,
			     Source::Trigger  &trigger,
			     Allocator        &alloc,
			     Multitouch const &multitouch,
			     Node       const &node)
			: Gesture(registry, alloc, node), _timer(timer), _trigger(trigger),
			  _attr(Attr::from_node(node)),
			  _multitouch(multitouch)
			{
				if (_attr.fingers > Multitouch::MAX_FINGERS)
					warning("Hold gesture limited to ",
					        (unsigned)Multitouch::MAX_FINGERS, " fingers");
			}
		};

		Registry<Gesture>   _gestures { };

		/**
		 * Filter interface
		 */
		void filter_event(Sink &destination, Input::Event const &event) override
		{
			Input::Event ev = event;

			bool active    { false };
			if (ev.touch() || ev.touch_release()) {
				struct Triggered_gesture { Gesture const &gesture; };
				Constructible<Triggered_gesture> triggered_gesture;

				_multitouch.handle_event(event);

				State old_state = _state;
				_gestures.for_each([&] (Gesture &gesture) {
					if (gesture.state() != old_state || triggered_gesture.constructed())
						return;

					gesture.handle_event(destination, ev);

					if (gesture.state() > _state)
						_state = gesture.state();

					switch (gesture.state()) {
						case TRIGGERED:
							gesture.generate(destination, _buffer);
							if (old_state != TRIGGERED)
								triggered_gesture.construct(Triggered_gesture { gesture });
							[[fallthrough]];
						case DETECT:
							active = true;
							break;
						case IDLE:
							break;
					}
				});

				/* cancel all other gestures if one gesture triggered */
				if (triggered_gesture.constructed()) {
					_gestures.for_each([&] (Gesture &gesture) {
						if (&gesture != &triggered_gesture->gesture)
							gesture.cancel();
					});
				}

				/* pass touch events if all gestures were cancelled */
				if (!active && _state != TRIGGERED) {
					_buffer.submit(destination);
					_buffer.clear();
					destination.submit(ev);
				}

			} else if (_state != DETECT) {
				/* pass all non-touch events in IDLE or TRIGGERED */
				destination.submit(ev);
			}

			if (active && _state == DETECT) {
				/* buffer all events if there is any gesture in DETECT */
				_buffer.store(ev);
			}

			ev.handle_touch_release([&] (Input::Touch_id) {
				/* cancel all gestures if all fingers have been released */
				if (_multitouch.fingers_present() == 0) {
					_state = State::IDLE;
					
					_gestures.for_each([&] (Gesture &gesture) {
						gesture.cancel(); });

					_buffer.submit(destination);
					_buffer.clear();
				}
			});

		}

	public:

		static char const *name() { return "touch-gesture"; }

		Touch_gesture_source(Owner &owner, Node const &config,
		                     Source::Factory &factory,
		                     Timer_accessor  &timer_accessor,
		                     Source::Trigger &trigger,
		                     Allocator       &alloc)
		:
			Source(owner),
			_owner(factory),
			_source(factory.create_source_for_sub_node(_owner, config)),
			_alloc(alloc)
		{
			config.for_each_sub_node("hold", [&] (Node const &node) {
				new (_alloc) Hold(_gestures, timer_accessor.timer(), trigger,
				                  _alloc, _multitouch, node);
			});

			config.for_each_sub_node("swipe", [&] (Node const &node) {
				new (_alloc) Swipe(_gestures, timer_accessor.timer(),
				                   _alloc, _multitouch, node);
			});
		}

		~Touch_gesture_source()
		{ 
			_gestures.for_each([&] (Gesture &gesture) {
				destroy(_alloc, &gesture); });
		}

		void generate(Sink &destination) override
		{
			if (_state == DETECT) {
				bool handled { false };
				_gestures.for_each([&] (Gesture &gesture) {
					if (handled || gesture.state() != TRIGGERED)
						return;

					gesture.generate(destination, _buffer);
					_state  = TRIGGERED;
					handled = true;
				});
			}

			Source::Filter::apply(destination, *this, _source);
		}
};

#endif /* _EVENT_FILTER__TOUCH_GESTURE_SOURCE_H_*/
