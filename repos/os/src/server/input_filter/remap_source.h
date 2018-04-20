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

#ifndef _INPUT_FILTER__REMAP_SOURCE_H_
#define _INPUT_FILTER__REMAP_SOURCE_H_

/* Genode includes */
#include <input/keycodes.h>

/* local includes */
#include <include_accessor.h>
#include <source.h>
#include <key_code_by_name.h>

namespace Input_filter { class Remap_source; }


class Input_filter::Remap_source : public Source, Source::Sink
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

		Source::Sink &_destination;

		/**
		 * Sink interface
		 */
		void submit_event(Input::Event const &event) override
		{
			using Input::Event;

			/* forward events that are unrelated to the remapper */
			if (!event.press() && !event.release()) {
				_destination.submit_event(event);
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
				_destination.submit_event(Input::Press_char{remap(key), codepoint}); });

			event.handle_release([&] (Input::Keycode key) {
				_destination.submit_event(Input::Release{remap(key)}); });
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
					Input::Keycode const code = key_code_by_name(key_name);

					if (node.has_attribute("to")) {
						Key_name const to = node.attribute_value("to", Key_name());
						try { _keys[code].code = key_code_by_name(to); }
						catch (Unknown_key) { warning("ignoring remap rule ", node); }
					}
				}
				catch (Unknown_key) {
					warning("invalid key name ", key_name); }
				return;
			}
		}

	public:

		static char const *name() { return "remap"; }

		Remap_source(Owner &owner, Xml_node config, Source::Sink &destination,
		             Source::Factory &factory, Include_accessor &include_accessor)
		:
			Source(owner),
			_include_accessor(include_accessor),
			_owner(factory),
			_source(factory.create_source(_owner, input_sub_node(config), *this)),
			_destination(destination)
		{
			for (unsigned i = 0; i < Input::KEY_MAX; i++)
				_keys[i].code = Input::Keycode(i);

			_apply_config(config);
		}

		void generate() override { _source.generate(); }
};

#endif /* _INPUT_FILTER__REMAP_SOURCE_H_ */
