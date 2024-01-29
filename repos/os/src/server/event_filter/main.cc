/*
 * \brief  Input-event filter
 * \author Norman Feske
 * \date   2017-02-01
 */

/*
 * Copyright (C) 2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <base/component.h>
#include <base/heap.h>
#include <timer_session/connection.h>
#include <event_session/connection.h>

/* local includes */
#include <input_source.h>
#include <remap_source.h>
#include <merge_source.h>
#include <chargen_source.h>
#include <button_scroll_source.h>
#include <accelerate_source.h>
#include <log_source.h>
#include <touch_click_source.h>
#include <touch_key_source.h>
#include <transform_source.h>
#include <event_session.h>

namespace Event_filter { struct Main; }


struct Event_filter::Main : Source::Factory, Source::Trigger
{
	Env &_env;

	Attached_rom_dataspace _config { _env, "config" };

	Heap _heap { _env.ram(), _env.rm() };

	Event_root _event_root { _env, _heap, *this, _config };

	/*
	 * Mechanism to construct a 'Timer' on demand
	 *
	 * By lazily constructing the timer, the event-filter does not depend
	 * on a timer service unless its configuration defines time-related
	 * filtering operations like key repeat.
	 */
	struct Timer_accessor : Event_filter::Timer_accessor
	{
		struct Lazy
		{
			Timer::Connection timer;

			Lazy(Env &env) : timer(env) { }
		};

		Env &_env;

		Constructible<Lazy> lazy { };

		Timer_accessor(Env &env) : _env(env) { }

		/**
		 * Timer_accessor interface
		 */
		Timer::Connection &timer() override
		{
			if (!lazy.constructed())
				lazy.construct(_env);

			return lazy->timer;
		}
	} _timer_accessor { _env };

	/**
	 * Pool of configuration include snippets, obtained as ROM modules
	 */
	struct Include_accessor : Event_filter::Include_accessor
	{
		struct Rom
		{
			typedef Include_accessor::Name Name;

			Registry<Rom>::Element    _reg_elem;
			Name             const    _name;
			Attached_rom_dataspace    _dataspace;
			Signal_context_capability _reconfig_sigh;

			void _handle_rom_update()
			{
				_dataspace.update();

				/* trigger reconfiguration */
				Signal_transmitter(_reconfig_sigh).submit();
			}

			Signal_handler<Rom> _rom_update_handler;

			Rom(Registry<Rom> &registry, Env &env, Name const &name,
			    Signal_context_capability reconfig_sigh)
			:
				_reg_elem(registry, *this), _name(name),
				_dataspace(env, name.string()), _reconfig_sigh(reconfig_sigh),
				_rom_update_handler(env.ep(), *this, &Rom::_handle_rom_update)
			{
				/* respond to ROM updates */
				_dataspace.sigh(_rom_update_handler);
			}

			bool has_name(Name const &name) const { return _name == name; }

			/**
			 * Return ROM content as XML
			 *
			 * \throw Include_unavailable
			 */
			Xml_node xml(Include_accessor::Type const &type) const
			{
				Xml_node const node = _dataspace.xml();
				if (node.type() == type)
					return node;

				warning("unexpected <", node.type(), "> node " "in included "
				        "ROM \"", _name, "\", expected, <", type, "> node");
				throw Include_unavailable();
			}
		};

		Env                      &_env;
		Allocator                &_alloc;
		Signal_context_capability _sigh;
		Registry<Rom>             _registry { };

		/**
		 * Return true if registry contains an include with the given name
		 */
		bool _exists(Rom::Name const &name)
		{
			bool exists = false;
			_registry.for_each([&] (Rom const &rom) {
				if (rom.has_name(name))
					exists = true; });

			return exists;
		}

		/**
		 * Constructor
		 *
		 * \param sigh  signal handler that responds to new ROM versions
		 */
		Include_accessor(Env &env, Allocator &alloc, Signal_context_capability sigh)
		:
			_env(env), _alloc(alloc), _sigh(sigh)
		{ }

		~Include_accessor()
		{
			_registry.for_each([&] (Rom &rom) { destroy(_alloc, &rom); });
		}

		void _apply_include(Name const &name, Type const &type, Functor const &fn) override
		{
			/* populate registry on demand */
			if (!_exists(name)) {
				try {
					Rom &rom = *new (_alloc) Rom(_registry, _env, name, _sigh);

					/* \throw Include_unavailable on mismatching top-level node type */
					rom.xml(type);
				}
				catch (...) { throw Include_unavailable(); }
			}

			/* call 'fn' with the XML content of the named include */
			Rom const *matching_rom = nullptr;
			_registry.for_each([&] (Rom const &rom) {
				if (rom.has_name(name))
					matching_rom = &rom; });

			/* this condition should never occur */
			if (!matching_rom)
				throw Include_unavailable();

			fn.apply(matching_rom->xml(type));
		}
	};

	/**
	 * Maximum nesting depth of input sources, for limiting the stack usage
	 */
	unsigned _create_source_max_nesting_level = 12;

	/**
	 * Source::Factory interface
	 *
	 * \throw Source::Invalid_config
	 */
	Source &create_source(Source::Owner &owner, Xml_node node) override
	{
		/*
		 * Guard for the protection against too deep recursions while
		 * processing the configuration.
		 */
		struct Nesting_level_guard
		{
			unsigned &level;

			Nesting_level_guard(unsigned &level) : level(level)
			{
				if (level == 0) {
					warning("too many nested input sources");
					throw Source::Invalid_config();
				}
				level--;
			}

			~Nesting_level_guard() { level++; }

		} nesting_level_guard { _create_source_max_nesting_level };

		/* return input source with the matching name */
		if (node.type() == Input_source::name())
			return *new (_heap)
				Input_source(owner,
				             node.attribute_value("name", Input_name()),
				             _event_root);

		/* create regular filter */
		if (node.type() == Remap_source::name())
			return *new (_heap) Remap_source(owner, node, *this,
			                                 _include_accessor);

		if (node.type() == Merge_source::name())
			return *new (_heap) Merge_source(owner, node, *this);

		if (node.type() == Chargen_source::name())
			return *new (_heap) Chargen_source(owner, node, *this, _heap,
			                                   _timer_accessor, _include_accessor,
			                                   *this);

		if (node.type() == Button_scroll_source::name())
			return *new (_heap) Button_scroll_source(owner, node, *this);

		if (node.type() == Accelerate_source::name())
			return *new (_heap) Accelerate_source(owner, node, *this);

		if (node.type() == Log_source::name())
			return *new (_heap) Log_source(owner, node, *this);

		if (node.type() == Transform_source::name())
			return *new (_heap) Transform_source(owner, node, *this);

		if (node.type() == Touch_click_source::name())
			return *new (_heap) Touch_click_source(owner, node, *this);

		if (node.type() == Touch_key_source::name())
			return *new (_heap) Touch_key_source(owner, node, *this, _heap);

		warning("unknown <", node.type(), "> input-source node type");
		throw Source::Invalid_config();
	}

	/**
	 * Source::Factory interface
	 */
	void destroy_source(Source &source) override { destroy(_heap, &source); }

	/*
	 * Flag used to defer configuration updates until all input sources are
	 * in their default state.
	 */
	bool _config_update_pending = false;

	struct Output
	{
		Source::Owner  _owner;
		Source        &_top_level;

		/**
		 * Constructor
		 *
		 * \throw Source::Invalid_config
		 * \throw Genode::Out_of_memory
		 */
		Output(Xml_node output, Source::Factory &factory)
		:
			_owner(factory),
			_top_level(factory.create_source(_owner, Source::input_sub_node(output)))
		{ }

		void generate(Source::Sink &destination) { _top_level.generate(destination); }
	};

	Constructible<Output> _output { };

	/* destination for filter results */
	Event::Connection _event_connection { _env };

	void _handle_config()
	{
		_config.update();

		bool const force = _config.xml().attribute_value("force", false);
		bool const idle  = _event_root.all_sessions_idle();

		/* defer reconfiguration until all sources are idle */
		if (!idle && !force) {
			_config_update_pending = true;
			return;
		}

		if (!idle)
			warning("force reconfiguration while input state is not idle");

		_apply_config();
	}

	void _apply_config()
	{
		Xml_node const config = _config.xml();

		_event_root.apply_config(config);

		try {
			if (config.has_sub_node("output"))
				_output.construct(config.sub_node("output"), *this);
		}
		catch (Source::Invalid_config) {
			warning("invalid <output> configuration"); }

		catch (Allocator::Out_of_memory) {
			error("out of memory while constructing filter chain"); }

		_config_update_pending = false;
	}

	Signal_handler<Main> _config_handler
		{ _env.ep(), *this, &Main::_handle_config };

	Include_accessor _include_accessor { _env, _heap, _config_handler };

	/**
	 * Source::Trigger interface
	 *
	 * Process pending events, which may originate from an event client or
	 * artificially emitted by a filter (character-repeat events).
	 */
	void trigger_generate() override
	{
		if (!_output.constructed())
			return;

		_event_connection.with_batch([&] (Event::Session_client::Batch &batch) {
			_output->generate(batch); });

		if (_config_update_pending && _event_root.all_sessions_idle())
			Signal_transmitter(_config_handler).submit();
	}

	/**
	 * Constructor
	 */
	Main(Genode::Env &env) : _env(env)
	{
		_config.sigh(_config_handler);

		/*
		 * Apply initial configuration
		 */
		_apply_config();

		/*
		 * Announce service
		 */
		_env.parent().announce(_env.ep().manage(_event_root));
	}
};


void Component::construct(Genode::Env &env) { static Event_filter::Main inst(env); }
