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
#include <source.h>
#include <key_code_by_name.h>

namespace Input_filter { class Remap_source; }


class Input_filter::Remap_source : public Source, Source::Sink
{
	private:

		struct Key
		{
			Input::Keycode code = Input::KEY_UNKNOWN;
			bool sticky = false;

			enum State { RELEASED, PRESSED } state = RELEASED;

			void toggle()
			{
				state = (state == PRESSED) ? RELEASED : PRESSED;
			}
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

			bool const key_event =
				event.type() == Event::PRESS || event.type() == Event::RELEASE;

			bool const code_valid =
				event.keycode() >= 0 && event.keycode() < Input::KEY_MAX;

			/* forward events that are unrelated to the remapper */
			if (!key_event || !code_valid) {
				_destination.submit_event(event);
				return;
			}

			Key &key = _keys[event.keycode()];

			Key::State const old_state = key.state;

			/* update key state, depending on the stickyness of the key */
			if (key.sticky) {
				if (event.type() == Event::PRESS)
					key.toggle();
			} else {
				key.state = (event.type() == Event::PRESS) ? Key::PRESSED
				                                           : Key::RELEASED;
			}

			/* drop release events of sticky keys */
			if (key.state == old_state)
				return;

			Event::Type const type =
				key.state == Key::PRESSED ? Event::PRESS : Event::RELEASE;

			_destination.submit_event(Event(type, key.code, 0, 0, 0, 0));
		}

	public:

		static char const *name() { return "remap"; }

		Remap_source(Owner &owner, Xml_node config, Source::Sink &destination,
		             Source::Factory &factory)
		:
			Source(owner),
			_owner(factory),
			_source(factory.create_source(_owner, input_sub_node(config), *this)),
			_destination(destination)
		{
			for (unsigned i = 0; i < Input::KEY_MAX; i++)
				_keys[i].code = Input::Keycode(i);

			config.for_each_sub_node("key", [&] (Xml_node node) {

				Key_name const key_name = node.attribute_value("name", Key_name());

				try {
					Input::Keycode const code = key_code_by_name(key_name);

					if (node.has_attribute("to")) {
						Key_name const to = node.attribute_value("to", Key_name());
						try { _keys[code].code = key_code_by_name(to); }
						catch (Unknown_key) { warning("ignoring remap rule ", node); }
					}

					_keys[code].sticky = node.attribute_value("sticky", false);
				}
				catch (Unknown_key) {
					warning("invalid key name ", key_name); }
			});
		}

		void generate() override { _source.generate(); }
};

#endif /* _INPUT_FILTER__REMAP_SOURCE_H_ */
