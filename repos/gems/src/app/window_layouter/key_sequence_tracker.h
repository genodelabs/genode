/*
 * \brief  Key seqyence tracker
 * \author Norman Feske
 * \date   2016-02-01
 */

/*
 * Copyright (C) 2016-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _KEY_SEQUENCE_TRACKER_H_
#define _KEY_SEQUENCE_TRACKER_H_

/* local includes */
#include <command.h>

namespace Window_layouter { class Key_sequence_tracker; }


class Window_layouter::Key_sequence_tracker
{
	private:

		struct Stack
		{
			struct Entry
			{
				bool press;

				Input::Keycode key;

				bool operator == (Entry const &other) const
				{
					return other.press == press && other.key == key;
				}

				void print(Output &out) const
				{
					Genode::print(out, press ? "press " : "release ", Input::key_name(key));
				}
			};

			/*
			 * Maximum number of consecutive press/release events in one key
			 * sequence.
			 */
			enum { MAX_ENTRIES = 64 };
			Entry entries[MAX_ENTRIES] { };

			unsigned pos = 0;

			void push(Entry entry)
			{
				entries[pos++] = entry;

				if (pos == MAX_ENTRIES) {
					warning("too long key sequence, dropping information");
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

			void print(Output &out) const
			{
				Genode::print(out, "[", pos, "]: ");
				for (unsigned i = 0; i < pos; i++)
					Genode::print(out, " ", entries[i]);
			}
		};

		Stack _stack { };

		void _with_matching_sub_node(Xml_node const &curr, Stack::Entry entry,
		                             auto const &fn, auto const &no_match_fn) const
		{
			auto const node_type = entry.press ? "press" : "release";

			using Key_name = String<32>;
			Key_name const key(Input::key_name(entry.key));

			bool done = false; /* process the first match only */
			curr.for_each_sub_node(node_type, [&] (Xml_node const &node) {
				if (!done && node.attribute_value("key", Key_name()) == key) {
					fn(node);
					done = true; } });

			if (!done)
				no_match_fn();
		}

		void _with_match_rec(unsigned const pos, Xml_node const &node, auto const &fn) const
		{
			if (pos == _stack.pos) {
				fn(node);
				return;
			}

			/* recursion is bounded by Stack::MAX_ENTRIES */
			_with_matching_sub_node(node, _stack.entries[pos],
				[&] (Xml_node const &sub_node) {
					if (pos < _stack.pos)
						_with_match_rec(pos + 1, sub_node, fn); },
				[&] { });
		};

		/**
		 * Call 'fn' with XML node that matches the state of the key sequence
		 *
		 * Traverse the nested '<press>' and '<release>' nodes of the
		 * configuration according to the history of events of the current
		 * sequence.
		 */
		void _with_xml_by_path(Xml_node const &config, auto const &fn) const
		{
			_with_match_rec(0, config, fn);
		}

		/**
		 * Execute command denoted in the specific XML node
		 */
		void _execute_command(Xml_node node, auto const &fn)
		{
			if (node.has_attribute("action"))
				fn(Command::from_xml(node));
		}

	public:

		/**
		 * Start new key sequence
		 */
		void reset() { _stack.reset(); }

		/**
		 * Apply event to key sequence
		 *
		 * \param fn  functor to be called if the event leads to a node in
		 *            the key-sequence configuration and the node is
		 *            equipped with an 'action' attribute. The functor is
		 *            called with an 'Action' as argument.
		 */
		void apply(Input::Event const &ev, Xml_node const &config, auto const &fn)
		{
			/*
			 * If the sequence contains a press-release combination for
			 * the pressed key, we flush those entries of the sequence
			 * to preserver the invariant that each key is present only
			 * once.
			 */
			ev.handle_press([&] (Input::Keycode key, Codepoint) {
				_stack.flush(Stack::Entry { .press = true,  .key = key });
				_stack.flush(Stack::Entry { .press = false, .key = key });
			});

			_with_xml_by_path(config, [&] (Xml_node const &curr_node) {

				ev.handle_press([&] (Input::Keycode key, Codepoint) {

					Stack::Entry const press { .press = true, .key = key };

					_with_matching_sub_node(curr_node, press,
						[&] (Xml_node const &node) { _execute_command(node, fn); },
						[&] { });

					_stack.push(press);
				});

				ev.handle_release([&] (Input::Keycode key) {

					Stack::Entry const release { .press = false, .key = key };

					/*
					 * If there exists a specific path for the release event,
					 * follow the path. Otherwise, we remove the released key
					 * from the sequence.
					 */
					_with_matching_sub_node(curr_node, release,
						[&] (Xml_node const &next_node) {
							_execute_command(next_node, fn);
							_stack.push(release);
						},
						[&] /* no match */ {
							Stack::Entry const press { .press = true, .key = key };
							_stack.flush(press);
						});
				});
			});
		}
};

#endif /* _KEY_SEQUENCE_TRACKER_H_ */
