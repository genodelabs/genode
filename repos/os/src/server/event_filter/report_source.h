/*
 * \brief  Input-event source that reports key combinations/shortcuts
 * \author Christian Helmuth
 * \date   2025-12-12
 */

/*
 * Copyright (C) 2025 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _EVENT_FILTER__REPORT_SOURCE_H_
#define _EVENT_FILTER__REPORT_SOURCE_H_

/* Genode includes */
#include <util/list_model.h>
#include <input/keycodes.h>

/* local includes */
#include <source.h>
#include <serial.h>
#include <key_code_by_name.h>

namespace Event_filter { class Report_source; }


class Event_filter::Report_source : public Source, Source::Filter
{
	private:

		Owner      _owner;
		Source    &_source;
		Allocator &_alloc;
		Env       &_env;
		Serial    &_serial;

		using Name = String<64>;

		static Name node_name(Node const &n, Name const &default_value = Name())
		{
			return n.attribute_value("name", default_value);
		}

		struct Keys
		{
			static constexpr size_t MAX = 8;

			unsigned num { };

			Input::Keycode keys[MAX] { };

			void reset()
			{
				num = 0;
				for (auto &k : keys) k = Input::KEY_RESERVED;
			}

			[[nodiscard]] bool push(Input::Keycode key)
			{
				if (num > MAX)
					return false;

				unsigned pos;
				for (pos = 0; pos < MAX; ++pos) {
					auto k = keys[pos];

					/* key already in combination */
					if (k == key)
						return false;

					/* skip to next */
					if (k != 0 && k < key)
						continue;

					break;
				}

				/* insert key at pos (move existing) */
				for (unsigned i = pos; i < MAX; ++i) {
					auto tmp = keys[i];
					keys[i]  = key;
					key      = tmp;
				}

				++num;
				return true;
			}

			void pop(Input::Keycode key)
			{
				if (num == 0)
					return;

				unsigned pos;
				for (pos = 0; pos < num; ++pos)
					if (keys[pos] == key)
						break;

				if (pos >= num)
					return;

				/* remove key at pos (fill with KEY_RESERVED) */
				for (unsigned i = pos; i < MAX; ++i)
					keys[i] = i + 1 < MAX
					        ? keys[i+1] : Input::KEY_RESERVED;
				--num;
			}

			bool operator == (Keys const &other) const
			{
				return num == other.num
				    && !memcmp(keys, other.keys, MAX);
			}
		};

		Keys _keys { };

		struct Shortcut : List_model<Shortcut>::Element
		{
			Name name;

			Expanding_reporter reporter;

			Keys keys { };

			Shortcut(Env &env, Node const &n)
			:
				name(node_name(n)), reporter(env, "shortcut", name)
			{ }

			void report(Keys const &cur, Serial &serial)
			{
				if (cur != keys) return;

				reporter.generate([&] (Generator &g) {
					g.attribute("name",   name);
					g.attribute("serial", serial.number());
				});
			}

			void update(Node const &n)
			{
				Keys new_keys;

				bool invalid = false;
				n.for_each_sub_node("key", [&] (auto const &k) {
					if (invalid) return;

					auto key_name = node_name(k, "KEY_UNKNOWN");

					invalid |= !n.has_attribute("name");
					invalid |= !new_keys.push(Input::key_code(key_name));
				});

				if (invalid)
					warning("ignoring invalid shortcut config: ", n);
				else
					keys = new_keys;
			}

			static bool type_matches(Node const &n)
			{
				return n.has_type("shortcut");
			}

			bool matches(Node const &n) const
			{
				return node_name(n) == name;
			}
		};

		List_model<Shortcut> _shortcuts { };

		void _apply_config(Node const &config)
		{
			_shortcuts.update_from_node(config,
				[&] (auto const &n) -> Shortcut & { return *new (_alloc) Shortcut(_env, n); },
				[&] (Shortcut &s)                 { destroy(_alloc, &s); },
				[&] (Shortcut &s, auto const &n)  { s.update(n); }
			);
		}

		/**
		 * Filter interface
		 */
		void filter_event(Source::Sink &destination, Input::Event const &event) override
		{
			event.handle_press([&] (Input::Keycode key, Codepoint) {
				/* ignore key presses that don't fit in combination */
				if (!_keys.push(key))
					return;

				_shortcuts.for_each([&] (auto &s) { s.report(_keys, _serial); });
			});

			event.handle_release([&] (Input::Keycode key) { _keys.pop(key); });

			/* forward event */
			destination.submit(event);
		}

	public:

		static char const *name() { return "report"; }

		Report_source(Owner             &owner,
		              Node        const &config,
		              Source::Factory   &factory,
		              Allocator         &alloc,
		              Env               &env,
		              Serial            &serial)
		:
			Source(owner),
			_owner(factory),
			_source(factory.create_source_for_sub_node(_owner, config)),
			_alloc(alloc), _env(env), _serial(serial)
		{
			_apply_config(config);
		}

		~Report_source() { _apply_config(Node()); }

		void generate(Source::Sink &destination) override
		{
			Source::Filter::apply(destination, *this, _source);
		}
};

#endif /* _EVENT_FILTER__REPORT_SOURCE_H_ */
