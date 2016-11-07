/*
 * \brief  Key seqyence tracker
 * \author Norman Feske
 * \date   2016-02-01
 */

/*
 * Copyright (C) 2016 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _KEY_SEQUENCE_TRACKER_H_
#define _KEY_SEQUENCE_TRACKER_H_

/* local includes */
#include "action.h"

namespace Floating_window_layouter { class Key_sequence_tracker; }


class Floating_window_layouter::Key_sequence_tracker
{
	private:

		enum State {

			/*
			 * No key is pressed. The next key will initiate a new key
			 * sequence.
			 */
			STATE_IDLE,

			/*
			 * Key sequence has been started.
			 */
			STATE_IN_SEQUENCE,
		};

		State _state = STATE_IDLE;

		struct Stack
		{
			struct Entry
			{
				enum Type { PRESS, RELEASE };

				Type type = PRESS;

				Input::Keycode keycode = Input::KEY_UNKNOWN;

				Entry() { }

				Entry(Type type, Input::Keycode keycode)
				: type(type), keycode(keycode) { }

				bool operator == (Entry const &other) const
				{
					return other.type == type && other.keycode == keycode;
				}
			};

			/*
			 * Maximum number of consecutive press/release events in one key
			 * sequence.
			 */
			enum { MAX_ENTRIES = 64 };
			Entry entries[MAX_ENTRIES];

			unsigned pos = 0;

			void push(Entry entry)
			{
				entries[pos++] = entry;

				if (pos == MAX_ENTRIES) {
					Genode::warning("too long key sequence, dropping information");
					pos = MAX_ENTRIES - 1;
				}
			}

			/**
			 * Removes highest matching entry from stack
			 */
			void flush(Entry entry)
			{
				/* detect attempt to flush key from empty stack */
				if (pos == 0)
					return;

				for (unsigned i = pos; i > 0; i--) {

					if (entries[i - 1] == entry) {

						/*
						 * Remove found entry by moving the subsequent entries
						 * by one position.
						 */
						for (unsigned j = i - 1; j < pos; j++)
							entries[j] = entries[j + 1];

						pos--;
						return;
					}
				}
			}

			void reset() { pos = 0; }
		};

		Stack _stack;

		Xml_node _matching_sub_node(Xml_node curr, Stack::Entry entry)
		{
			char const *node_type = entry.type == Stack::Entry::PRESS
			                      ? "press" : "release";

			typedef Genode::String<32> Key_name;
			Key_name const key(Input::key_name(entry.keycode));

			Xml_node result("<none/>");

			curr.for_each_sub_node(node_type, [&] (Xml_node const &node) {

				if (node.attribute_value("key", Key_name()) != key)
					return;

				/* set 'result' only once, so we return the first match */
				if (result.has_type("none"))
					result = node;
			});

			return result;
		}

		/**
		 * Lookup XML node that matches the state of the key sequence
		 *
		 * Traverse the nested '<press>' and '<release>' nodes of the
		 * configuration according to the history of events of the current
		 * sequence.
		 *
		 * \return XML node of the type '<press>' or '<release>'.
		 *         If the configuration does not contain a matching node, the
		 *         method returns a dummy node '<none>'.
		 */
		Xml_node _xml_by_path(Xml_node config)
		{
			Xml_node curr = config;

			/*
			 * Each iteration corresponds to a nesting level
			 */
			for (unsigned i = 0; i < _stack.pos; i++) {

				Stack::Entry const entry = _stack.entries[i];

				Xml_node const match = _matching_sub_node(curr, entry);

				if (match.has_type("none"))
					return match;

				curr = match;
			}

			return curr;
		}

		/**
		 * Execute action denoted in the specific XML node
		 */
		template <typename FUNC>
		void _execute_action(Xml_node node, FUNC const &func)
		{
			if (!node.has_attribute("action"))
				return;

			typedef String<32> Action;
			Action action = node.attribute_value("action", Action());

			func(Floating_window_layouter::Action(action));
		}

	public:

		/**
		 * Start new key sequence
		 */
		void reset() { _stack.reset(); }

		/**
		 * Apply event to key sequence
		 *
		 * \param func  functor to be called if the event leads to a node in
		 *              the key-sequence configuration and the node is
		 *              equipped with an 'action' attribute. The functor is
		 *              called with an 'Action' as argument.
		 */
		template <typename FUNC>
		void apply(Input::Event const &ev, Xml_node config, FUNC const &func)
		{
			/*
			 * If the sequence contains a press-release combination for
			 * the pressed key, we flush those entries of the sequence
			 * to preserver the invariant that each key is present only
			 * once.
			 */
			if (ev.type() == Input::Event::PRESS) {
				_stack.flush(Stack::Entry(Stack::Entry::PRESS,   ev.keycode()));
				_stack.flush(Stack::Entry(Stack::Entry::RELEASE, ev.keycode()));
			}

			Xml_node curr_node = _xml_by_path(config);

			if (ev.type() == Input::Event::PRESS) {

				Stack::Entry const entry(Stack::Entry::PRESS, ev.keycode());

				_execute_action(_matching_sub_node(curr_node, entry), func);
				_stack.push(entry);
			}

			if (ev.type() == Input::Event::RELEASE) {

				Stack::Entry const entry(Stack::Entry::RELEASE, ev.keycode());

				Xml_node const next_node = _matching_sub_node(curr_node, entry);

				/*
				 * If there exists a specific path for the release event,
				 * follow the path. Otherwise, we remove the released key from
				 * the sequence.
				 */
				if (!next_node.has_type("none")) {

					_execute_action(next_node, func);
					_stack.push(entry);

				} else {

					Stack::Entry entry(Stack::Entry::PRESS, ev.keycode());
					_stack.flush(entry);
				}
			}
		}
};


#endif /* _KEY_SEQUENCE_TRACKER_H_ */
