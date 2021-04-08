/*
 * \brief  Input-event source that remaps keys from another source
 * \author Norman Feske
 * \date   2017-02-01
 */

/*
 * Copyright (C) 2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _EVENT_FILTER__REMAP_SOURCE_H_
#define _EVENT_FILTER__REMAP_SOURCE_H_

/* Genode includes */
#include <input/keycodes.h>

/* local includes */
#include <include_accessor.h>
#include <source.h>
#include <key_code_by_name.h>

namespace Event_filter { class Remap_source; }


class Event_filter::Remap_source : public Source, Source::Filter
{
	private:

		Include_accessor &_include_accessor;

		struct Key
		{
			Input::Keycode code = Input::KEY_UNKNOWN;
		};

		Key _keys[Input::KEY_MAX];

		Owner _owner;

		Source &_source;

		/**
		 * Filter interface
		 */
		void filter_event(Source::Sink &destination, Input::Event const &event) override
		{
			using Input::Event;

			/* forward events that are unrelated to the remapper */
			if (!event.press() && !event.release()) {
				destination.submit(event);
				return;
			}

			/*
			 * remap the key code for press and release events
			 *
			 * The range of the 'key' is checked by the 'Event' handle methods,
			 * so it is safe to use as array index.
			 */
			auto remap = [&] (Input::Keycode key) { return _keys[key].code; };

			event.handle_press([&] (Input::Keycode key, Codepoint codepoint) {
				destination.submit(Input::Press_char{remap(key), codepoint}); });

			event.handle_release([&] (Input::Keycode key) {
				destination.submit(Input::Release{remap(key)}); });
		}

		void _apply_config(Xml_node const config, unsigned const max_recursion = 4)
		{
			config.for_each_sub_node([&] (Xml_node node) {
				_apply_sub_node(node, max_recursion); });
		}

		void _apply_sub_node(Xml_node const node, unsigned const max_recursion)
		{
			if (max_recursion == 0) {
				warning("too deeply nested includes");
				throw Invalid_config();
			}

			/*
			 * Handle includes
			 */
			if (node.type() == "include") {
				try {
					Include_accessor::Name const rom =
						node.attribute_value("rom", Include_accessor::Name());

					_include_accessor.apply_include(rom, name(), [&] (Xml_node inc) {
						_apply_config(inc, max_recursion - 1); });
					return;
				}
				catch (Include_accessor::Include_unavailable) {
					throw Invalid_config(); }
			}

			/*
			 * Handle key nodes
			 */
			if (node.type() == "key") {
				Key_name const key_name = node.attribute_value("name", Key_name());

				try {
					for_each_key_with_name(key_name, [&] (Input::Keycode code) {
						if (node.has_attribute("to")) {
							Key_name const to = node.attribute_value("to", Key_name());
							try { _keys[code].code = key_code_by_name(to); }
							catch (Unknown_key) { warning("ignoring remap rule ", node); }
						}
					});
				}
				catch (Unknown_key) {
					warning("invalid key name ", key_name); }
				return;
			}
		}

	public:

		static char const *name() { return "remap"; }

		Remap_source(Owner &owner, Xml_node config, Source::Factory &factory,
		             Include_accessor &include_accessor)
		:
			Source(owner),
			_include_accessor(include_accessor), _owner(factory),
			_source(factory.create_source(_owner, input_sub_node(config)))
		{
			for (unsigned i = 0; i < Input::KEY_MAX; i++)
				_keys[i].code = Input::Keycode(i);

			_apply_config(config);
		}

		void generate(Source::Sink &destination) override
		{
			Source::Filter::apply(destination, *this, _source);
		}
};

#endif /* _EVENT_FILTER__REMAP_SOURCE_H_ */
