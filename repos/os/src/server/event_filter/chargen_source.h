/*
 * \brief  Input-event source that generates character events
 * \author Norman Feske
 * \date   2017-02-01
 */

/*
 * Copyright (C) 2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _EVENT_FILTER__CHARGEN_SOURCE_H_
#define _EVENT_FILTER__CHARGEN_SOURCE_H_

/* Genode includes */
#include <input/keycodes.h>

/* local includes */
#include <source.h>
#include <timer_accessor.h>
#include <include_accessor.h>

namespace Event_filter { class Chargen_source; }


class Event_filter::Chargen_source : public Source, Source::Filter
{
	private:

		typedef Input::Event Event;

		Allocator        &_alloc;
		Timer_accessor   &_timer_accessor;
		Include_accessor &_include_accessor;

		/*
		 * Modifier definitions
		 */

		struct Modifier
		{
			enum Id { MOD1 = 0, MOD2 = 1, MOD3 = 2, MOD4 = 3, UNDEFINED };

			typedef String<8> Name;

			Registry<Modifier>::Element _element;

			Id const _id;

			Input::Keycode const _code;

			static Id id(Xml_node mod_node)
			{
				if (mod_node.type() == "mod1") return MOD1;
				if (mod_node.type() == "mod2") return MOD2;
				if (mod_node.type() == "mod3") return MOD3;
				if (mod_node.type() == "mod4") return MOD4;

				return UNDEFINED;
			}

			Modifier(Registry<Modifier> &registry, Id id, Input::Keycode code)
			:
				_element(registry, *this), _id(id), _code(code)
			{ }

			Input::Keycode code() const { return _code; }

			Id id() const { return _id; }
		};

		Registry<Modifier> _modifiers { };

		struct Modifier_rom
		{
			typedef String<32> Name;

			Registry<Modifier_rom>::Element _element;

			Modifier::Id const _id;

			bool _enabled = false;

			Modifier_rom(Registry<Modifier_rom> &registry,
			             Modifier::Id            id,
			             Include_accessor       &include_accessor,
			             Name             const &name)
			:
				_element(registry, *this), _id(id)
			{
				try {
					include_accessor.apply_include(name, "capslock", [&] (Xml_node node) {
						_enabled = node.attribute_value("enabled", false); }); }

				catch (Include_accessor::Include_unavailable) {
					warning("failed to obtain modifier state from "
					        "\"", name, "\" ROM module"); }
			}

			Modifier::Id id() const { return _id; }

			bool enabled() const { return _enabled; }
		};

		Registry<Modifier_rom> _modifier_roms { };

		/*
		 * Key rules for generating characters
		 */

		enum { NUM_MODIFIERS = 4 };

		/**
		 * Cached state of modifiers, updated when a modifier key event occurs
		 */
		struct Modifier_map
		{
			struct State { bool enabled = false; } states[NUM_MODIFIERS];

		} _mod_map { };

		/**
		 * State tracked per physical key
		 */
		struct Key
		{
			enum Type  { DEFAULT, MODIFIER } type  = DEFAULT;
			enum State { RELEASED, PRESSED } state = RELEASED;

			struct Rule
			{
				Registry<Rule>::Element _reg_elem;

				/*
				 * Conditions that must be satisfied to let the rule take effect
				 */
				struct Conditions
				{
					struct Modifier
					{
						enum Constraint { PRESSED, RELEASED, DONT_CARE };

						Constraint constraint = DONT_CARE;

						bool match(Modifier_map::State state) const
						{
							if ((constraint == RELEASED &&  state.enabled) ||
								(constraint == PRESSED  && !state.enabled))
								return false;

							return true;
						}
					};

					Modifier modifiers[NUM_MODIFIERS];

					/**
					 * Return true if current modifier state fulfils conditions
					 */
					bool match(Modifier_map const &mod_map) const
					{
						for (unsigned i = 0; i < NUM_MODIFIERS; i++)
							if (!modifiers[i].match(mod_map.states[i]))
								return false;

						return true;
					}

					unsigned num_modifier_constraints() const
					{
						unsigned cnt = 0;
						for (unsigned i = 0; i < NUM_MODIFIERS; i++)
							if (modifiers[i].constraint != Modifier::DONT_CARE)
								cnt++;

						return cnt;
					}
				};

				Conditions const _conditions;

				Codepoint const _character;

				Rule(Registry<Rule> &registry,
				     Conditions      conditions,
				     Codepoint       character)
				:
					_reg_elem(registry, *this),
					_conditions(conditions),
					_character(character)
				{ }

				/**
				 * Return match score for the given modifier state
				 *
				 * \return 0    if rule mismatches,
				 *         1    if rule matches,
				 *         1+N  if rule with N modifier constraints matches
				 */
				unsigned match_score(Modifier_map const &mod_map) const
				{
					if (!_conditions.match(mod_map))
						return 0;

					return 1 + _conditions.num_modifier_constraints();
				}

				Codepoint character() const { return _character; }
			};

			Registry<Rule> rules { };

			/**
			 * Call functor 'fn' with the codepoint of the character defined
			 * for the best matching rule
			 */
			template <typename FN>
			void apply_best_matching_rule(Modifier_map const &mod_map, FN const &fn) const
			{
				Codepoint best_match { Codepoint::INVALID };

				unsigned max_score = 0;

				rules.for_each([&] (Rule const &rule) {

					unsigned score = rule.match_score(mod_map);
					if (score <= max_score)
						return;

					max_score = score;
					best_match = rule.character();
				});

				if (max_score > 0)
					fn(best_match);
			}
		};

		struct Missing_character_definition { };

		/**
		 * Return Unicode codepoint defined in XML node attributes
		 *
		 * \throw Missing_character_definition
		 */
		static Codepoint _codepoint_from_xml_node(Xml_node node)
		{
			if (node.has_attribute("ascii"))
				return Codepoint { node.attribute_value<uint32_t>("ascii", 0) };

			if (node.has_attribute("code"))
				return Codepoint { node.attribute_value<uint32_t>("code", 0) };

			if (node.has_attribute("char")) {

				typedef String<2> Value;
				Value value = node.attribute_value("char", Value());

				unsigned char const ascii = value.string()[0];

				if (ascii < 128)
					return Codepoint { ascii };

				warning("char attribute with non-ascii character "
				        "'", value, "'");
				throw Missing_character_definition();
			}

			if (node.has_attribute("b0")) {
				char const b0 = node.attribute_value("b0", 0L),
				           b1 = node.attribute_value("b1", 0L),
				           b2 = node.attribute_value("b2", 0L),
				           b3 = node.attribute_value("b3", 0L);

				char const buf[5] { b0, b1, b2, b3, 0 };
				return Utf8_ptr(buf).codepoint();
			}

			throw Missing_character_definition();
		}

		/**
		 * Map of the states of the physical keys
		 */
		class Key_map
		{
			private:

				Allocator &_alloc;

				Key _keys[Input::KEY_MAX];

			public:

				Key_map(Allocator &alloc) : _alloc(alloc) { }

				~Key_map()
				{
					for (unsigned i = 0; i < Input::KEY_MAX; i++)
						_keys[i].rules.for_each([&] (Key::Rule &rule) {
							destroy(_alloc, &rule); });
				}

				/**
				 * Return key object that belongs to the specified key code
				 */
				Key &key(Input::Keycode code)
				{
					if ((unsigned)code >= (unsigned)Input::KEY_MAX)
						return _keys[Input::KEY_UNKNOWN];

					return _keys[code];
				};

				/**
				 * Obtain modifier condition from map XML node
				 */
				static Key::Rule::Conditions::Modifier::Constraint
				_map_mod_cond(Xml_node map, Modifier::Name const &mod_name)
				{
					if (!map.has_attribute(mod_name.string()))
						return Key::Rule::Conditions::Modifier::DONT_CARE;

					bool const pressed = map.attribute_value(mod_name.string(), false);

					return pressed ? Key::Rule::Conditions::Modifier::PRESSED
					               : Key::Rule::Conditions::Modifier::RELEASED;
				}

				void import_map(Xml_node map)
				{
					/* obtain modifier conditions from map attributes */
					Key::Rule::Conditions cond;
					cond.modifiers[Modifier::MOD1].constraint = _map_mod_cond(map, "mod1");
					cond.modifiers[Modifier::MOD2].constraint = _map_mod_cond(map, "mod2");
					cond.modifiers[Modifier::MOD3].constraint = _map_mod_cond(map, "mod3");
					cond.modifiers[Modifier::MOD4].constraint = _map_mod_cond(map, "mod4");

					/* add a rule for each <key> sub node */
					map.for_each_sub_node("key", [&] (Xml_node key_node) {

						Key_name const name = key_node.attribute_value("name", Key_name());

						Input::Keycode const code = key_code_by_name(name);

						new (_alloc) Key::Rule(key(code).rules, cond,
						                       _codepoint_from_xml_node(key_node));
					});
				}

		} _key_map;

		void _update_modifier_state()
		{
			/* reset */
			_mod_map = Modifier_map();

			/* apply state of all modifier keys to modifier map */
			_modifiers.for_each([&] (Modifier const &mod) {
				_mod_map.states[mod.id()].enabled |=
					_key_map.key(mod.code()).state; });

			/* supplement modifier state provided by ROM modules */
			_modifier_roms.for_each([&] (Modifier_rom const &mod_rom) {
				_mod_map.states[mod_rom.id()].enabled |=
					mod_rom.enabled(); });
		}

		/**
		 * Generate characters from codepoint sequences
		 */
		class Sequencer
		{
			private:

				Allocator &_alloc;

				struct Sequence
				{
					Codepoint seq[4] { {Codepoint::INVALID}, {Codepoint::INVALID},
						           {Codepoint::INVALID}, {Codepoint::INVALID} };

					unsigned len { 0 };

					enum Match { MISMATCH , UNFINISHED, COMPLETED };

					Sequence() { }

					Sequence(Codepoint c0, Codepoint c1, Codepoint c2, Codepoint c3)
					: seq { c0, c1, c2, c3 }, len { 4 } { }

					void append(Codepoint c)
					{
						/* excess codepoints are just dropped */
						if (len < 4)
							seq[len++] = c;
					}

					/**
					 * Match 'other' to 'this' until first invalid codepoint in
					 * 'other', completion, or mismatch
					 */
					Match match(Sequence const &o) const
					{
						/* first codepoint must match */
						if (o.seq[0].value != seq[0].value) return MISMATCH;

						for (unsigned i = 1; i < sizeof(seq)/sizeof(*seq); ++i) {
							/* end of this sequence means COMPLETED */
							if (!seq[i].valid()) break;

							/* end of other sequence means UNFINISHED */
							if (!o.seq[i].valid()) return UNFINISHED;

							if (o.seq[i].value != seq[i].value) return MISMATCH;

							/* continue until completion with both valid and equal */
						}
						return COMPLETED;
					}
				};

				struct Rule
				{
					typedef Sequence::Match Match;

					Registry<Rule>::Element element;
					Sequence          const sequence;
					Codepoint         const code;

					Rule(Registry<Rule> &registry, Sequence const &sequence, Codepoint code)
					:
						element(registry, *this),
						sequence(sequence),
						code(code)
					{ }
				};

				Registry<Rule> _rules { };

				Sequence _curr_sequence { };

			public:

				Sequencer(Allocator &alloc) : _alloc(alloc) { }

				~Sequencer()
				{
					_rules.for_each([&] (Rule &rule) {
						destroy(_alloc, &rule); });
				}

				void import_sequence(Xml_node node)
				{
					unsigned const invalid { Codepoint::INVALID };

					Sequence sequence {
						Codepoint { node.attribute_value("first",  invalid) },
						Codepoint { node.attribute_value("second", invalid) },
						Codepoint { node.attribute_value("third",  invalid) },
						Codepoint { node.attribute_value("fourth", invalid) } };

					new (_alloc) Rule(_rules, sequence, _codepoint_from_xml_node(node));
				}

				Codepoint process(Codepoint codepoint)
				{
					Codepoint const invalid { Codepoint::INVALID };
					Rule::Match best_match  { Sequence::MISMATCH };
					Codepoint result        { codepoint };
					Sequence seq            { _curr_sequence };

					seq.append(codepoint);

					_rules.for_each([&] (Rule const &rule) {
						/* early return if completed match was found already */
						if (best_match == Sequence::COMPLETED) return;

						Rule::Match const match { rule.sequence.match(seq) };
						switch (match) {
						case Sequence::MISMATCH:
							return;
						case Sequence::UNFINISHED:
							best_match = match;
							result     = invalid;
							return;
						case Sequence::COMPLETED:
							best_match = match;
							result     = rule.code;
							return;
						}
					});

					switch (best_match) {
					case Sequence::MISMATCH:
						/* drop cancellation codepoint of unfinished sequence */
						if (_curr_sequence.len > 0)
							result = invalid;
						_curr_sequence = Sequence();
						break;
					case Sequence::UNFINISHED:
						_curr_sequence = seq;
						break;
					case Sequence::COMPLETED:
						_curr_sequence = Sequence();
						break;
					}

					return result;
				}
		} _sequencer;

		Owner _owner;

		/**
		 * Mechanism for periodically repeating the last character
		 */
		struct Char_repeater
		{
			Timer::Connection &_timer;
			Source::Trigger   &_trigger;

			Microseconds const _delay;
			Microseconds const _rate;

			unsigned _pending_event_count = 0;

			Codepoint _curr_character { Codepoint::INVALID };

			enum State { IDLE, REPEAT } _state { IDLE };

			void _handle_timeout(Duration)
			{
				if (_state == REPEAT) {
					_pending_event_count++;
					_timeout.schedule(_rate);
				}

				_trigger.trigger_generate();
			}

			void emit_events(Source::Sink &destination)
			{
				for (unsigned i = 0; i < _pending_event_count; i++) {
					destination.submit(Input::Press_char{Input::KEY_UNKNOWN,
					                                      _curr_character});
					destination.submit(Input::Release{Input::KEY_UNKNOWN});
				}

				_pending_event_count = 0;
			}

			Timer::One_shot_timeout<Char_repeater> _timeout {
				_timer, *this, &Char_repeater::_handle_timeout };

			Char_repeater(Timer::Connection &timer, Xml_node node, Source::Trigger &trigger)
			:
				_timer(timer), _trigger(trigger),
				_delay(node.attribute_value("delay_ms", (uint64_t)0)*1000),
				_rate (node.attribute_value("rate_ms",  (uint64_t)0)*1000)
			{ }

			void schedule_repeat(Codepoint character)
			{
				_curr_character      = character;
				_state               = REPEAT;
				_pending_event_count = 0;

				_timeout.schedule(_delay);
			}

			void cancel()
			{
				_curr_character      = Codepoint { Codepoint::INVALID };
				_state               = IDLE;
				_pending_event_count = 0;
			}
		};

		Constructible<Char_repeater> _char_repeater { };

		/**
		 * Filter interface
		 */
		void filter_event(Sink &destination, Event const &event) override
		{
			Event ev = event;

			ev.handle_press([&] (Input::Keycode keycode, Codepoint /* ignored */) {

				Key &key = _key_map.key(keycode);
				key.state = Key::PRESSED;

				if (key.type == Key::MODIFIER) _update_modifier_state();

				/* supplement codepoint information to press event */
				key.apply_best_matching_rule(_mod_map, [&] (Codepoint codepoint) {

					codepoint = _sequencer.process(codepoint);

					ev = Event(Input::Press_char{keycode, codepoint});

					if (_char_repeater.constructed())
						_char_repeater->schedule_repeat(codepoint);
				});
			});

			ev.handle_release([&] (Input::Keycode keycode) {

				Key &key = _key_map.key(keycode);
				key.state = Key::RELEASED;

				if (key.type == Key::MODIFIER) _update_modifier_state();

				if (_char_repeater.constructed())
					_char_repeater->cancel();
			});

			/* forward filtered event */
			destination.submit(ev);
		}

		Source &_source;

		Source::Trigger &_trigger;

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
			 * Handle map nodes
			 */
			if (node.type() == "map") {
				try {
					_key_map.import_map(node);
					return;
				}
				catch (Missing_character_definition) {
					throw Invalid_config(); }
			}

			/*
			 * Handle sequence nodes
			 */
			if (node.type() == "sequence") {
				try {
					_sequencer.import_sequence(node);
					return;
				}
				catch (Missing_character_definition) {
					throw Invalid_config(); }
			}

			/*
			 * Instantiate character repeater on demand
			 */
			if (node.type() == "repeat") {
				_char_repeater.construct(_timer_accessor.timer(), node, _trigger);
				return;
			}

			/*
			 * Handle modifier-definition nodes
			 */
			Modifier::Id const id = Modifier::id(node);
			if (id == Modifier::UNDEFINED)
				return;

			node.for_each_sub_node("key", [&] (Xml_node key_node) {

				Key_name const name = key_node.attribute_value("name", Key_name());
				Input::Keycode const key = key_code_by_name(name);

				new (_alloc) Modifier(_modifiers, id, key);
			});

			node.for_each_sub_node("rom", [&] (Xml_node rom_node) {

				typedef Modifier_rom::Name Rom_name;
				Rom_name const rom_name = rom_node.attribute_value("name", Rom_name());

				new (_alloc) Modifier_rom(_modifier_roms, id, _include_accessor, rom_name);
			});

			_update_modifier_state();
		}

	public:

		static char const *name() { return "chargen"; }

		Chargen_source(Owner            &owner,
		               Xml_node          config,
		               Source::Factory  &factory,
		               Allocator        &alloc,
		               Timer_accessor   &timer_accessor,
		               Include_accessor &include_accessor,
		               Source::Trigger  &trigger)
		:
			Source(owner),
			_alloc(alloc),
			_timer_accessor(timer_accessor),
			_include_accessor(include_accessor),
			_key_map(_alloc),
			_sequencer(_alloc),
			_owner(factory),
			_source(factory.create_source(_owner, input_sub_node(config))),
			_trigger(trigger)
		{
			_apply_config(config);

			/* assign key types in key map */
			_modifiers.for_each([&] (Modifier const &mod) {
				_key_map.key(mod.code()).type = Key::MODIFIER; });
		}

		~Chargen_source()
		{
			_modifiers.for_each([&] (Modifier &mod) {
				destroy(_alloc, &mod); });

			_modifier_roms.for_each([&] (Modifier_rom &mod_rom) {
				destroy(_alloc, &mod_rom); });
		}

		void generate(Sink &destination) override
		{
			if (_char_repeater.constructed())
				_char_repeater->emit_events(destination);

			Source::Filter::apply(destination, *this, _source);

		}
};

#endif /* _EVENT_FILTER__CHARGEN_SOURCE_H_ */
