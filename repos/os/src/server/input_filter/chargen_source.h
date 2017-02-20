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

#ifndef _INPUT_FILTER__CHARGEN_SOURCE_H_
#define _INPUT_FILTER__CHARGEN_SOURCE_H_

/* Genode includes */
#include <input/keycodes.h>

/* local includes */
#include <source.h>
#include <timer_accessor.h>
#include <include_accessor.h>

namespace Input_filter { class Chargen_source; }


class Input_filter::Chargen_source : public Source, Source::Sink
{
	private:

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

		Registry<Modifier> _modifiers;

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

		} _mod_map;

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

				Input::Event::Utf8 const _character;

				Rule(Registry<Rule>    &registry,
				     Conditions         conditions,
				     Input::Event::Utf8 character)
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

				Input::Event::Utf8 character() const { return _character; }
			};

			Registry<Rule> rules;

			/**
			 * Call functor 'fn' with the 'Input::Event::Utf8' character
			 * defined for the best matching rule
			 */
			template <typename FN>
			void apply_best_matching_rule(Modifier_map const &mod_map, FN const &fn) const
			{
				Input::Event::Utf8 best_match { 0 };

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

				struct Missing_character_definition { };

				/**
				 * Return UTF8 character defined in XML node attributes
				 *
				 * \throw Missing_character_definition
				 */
				static Input::Event::Utf8 _utf8_from_xml_node(Xml_node node)
				{
					if (node.has_attribute("ascii"))
						return Input::Event::Utf8(node.attribute_value("ascii", 0UL));

					if (node.has_attribute("char")) {

						typedef String<2> Value;
						Value value = node.attribute_value("char", Value());

						unsigned char const ascii = value.string()[0];

						if (ascii < 128)
							return Input::Event::Utf8(ascii);

						warning("char attribute with non-ascii character "
						        "'", value, "'");
						throw Missing_character_definition();
					}

					if (node.has_attribute("b0")) {
						unsigned char const b0 = node.attribute_value("b0", 0UL),
						                    b1 = node.attribute_value("b1", 0UL),
						                    b2 = node.attribute_value("b2", 0UL),
						                    b3 = node.attribute_value("b3", 0UL);

						return Input::Event::Utf8(b0, b1, b2, b3);
					}

					throw Missing_character_definition();
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
						                       _utf8_from_xml_node(key_node));
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
		}

		Owner _owner;

		Source::Sink &_destination;

		/**
		 * Mechanism for periodically repeating the last character
		 */
		struct Char_repeater
		{
			Source::Sink  &_destination;
			Genode::Timer &_timer;

			Time_source::Microseconds const _delay;
			Time_source::Microseconds const _rate;

			Input::Event::Utf8 _curr_character { 0 };

			enum State { IDLE, REPEAT } _state;

			void _handle_timeout(Time_source::Microseconds)
			{
				if (_state == REPEAT) {
					_destination.submit_event(Input::Event(_curr_character));
					_timeout.start(_rate);
				}
			}

			One_shot_timeout<Char_repeater> _timeout {
				_timer, *this, &Char_repeater::_handle_timeout };

			Char_repeater(Source::Sink &destination, Genode::Timer &timer,
			              Xml_node node)
			:
				_destination(destination), _timer(timer),
				_delay(node.attribute_value("delay_ms", 0UL)*1000),
				_rate (node.attribute_value("rate_ms",  0UL)*1000)
			{ }

			void schedule_repeat(Input::Event::Utf8 character)
			{
				_curr_character = character;
				_state          = REPEAT;

				_timeout.start(_delay);
			}

			void cancel()
			{
				_curr_character = Input::Event::Utf8(0);
				_state          = IDLE;
			}
		};

		Constructible<Char_repeater> _char_repeater;

		/**
		 * Sink interface (called from our child node)
		 */
		void submit_event(Input::Event const &event) override
		{
			using Input::Event;

			/* forward event as is */
			_destination.submit_event(event);

			/* don't do anything for non-press/release events */
			if (event.type() != Event::PRESS && event.type() != Event::RELEASE)
				return;

			Key &key = _key_map.key(event.keycode());

			/* track key state */
			if (event.type() == Event::PRESS)   key.state = Key::PRESSED;
			if (event.type() == Event::RELEASE) key.state = Key::RELEASED;

			if (key.type == Key::MODIFIER) {
				_update_modifier_state();

				/* never emit a character when pressing a modifier key */
				return;
			}

			if (event.type() == Event::PRESS) {
				key.apply_best_matching_rule(_mod_map, [&] (Event::Utf8 utf8) {

					_destination.submit_event(Event(utf8));

					if (_char_repeater.constructed())
						_char_repeater->schedule_repeat(utf8);
				});
			}

			if (event.type() == Event::RELEASE)
				if (_char_repeater.constructed())
					_char_repeater->cancel();
		}

		Source &_source;

		void _apply_config(Xml_node const config, unsigned const max_recursion = 4)
		{
			config.for_each_sub_node([&] (Xml_node node) {
				_apply_sub_node(node, max_recursion); });
		}

		void _apply_sub_node(Xml_node const node, unsigned const max_recursion)
		{
			if (max_recursion == 0) {
				error("too deeply nested includes");
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
				_key_map.import_map(node);
				return;
			}

			/*
			 * Instantiate character repeater on demand
			 */
			if (node.type() == "repeat") {
				_char_repeater.construct(_destination,
				                         _timer_accessor.timer(), node);
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
		}

	public:

		static char const *name() { return "chargen"; }

		Chargen_source(Owner            &owner,
		               Xml_node          config,
		               Source::Sink     &destination,
		               Source::Factory  &factory,
		               Allocator        &alloc,
		               Timer_accessor   &timer_accessor,
		               Include_accessor &include_accessor)
		:
			Source(owner),
			_alloc(alloc),
			_timer_accessor(timer_accessor),
			_include_accessor(include_accessor),
			_key_map(_alloc),
			_owner(factory),
			_destination(destination),
			_source(factory.create_source(_owner, input_sub_node(config), *this))
		{
			_apply_config(config);

			/* assign key types in key map */
			_modifiers.for_each([&] (Modifier const &mod) {
				_key_map.key(mod.code()).type = Key::MODIFIER; });
		}

		~Chargen_source()
		{
			_modifiers.for_each([&] (Modifier &mod) { destroy(_alloc, &mod); });
		}

		void generate() override { _source.generate(); }
};

#endif /* _INPUT_FILTER__CHARGEN_SOURCE_H_ */
