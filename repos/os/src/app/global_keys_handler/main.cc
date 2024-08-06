/*
 * \brief  Utility for generating state reports from global key events
 * \author Norman Feske
 * \date   2015-10-03
 */

/*
 * Copyright (C) 2015-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <base/component.h>
#include <base/heap.h>
#include <base/attached_rom_dataspace.h>
#include <base/registry.h>
#include <gui_session/connection.h>
#include <os/reporter.h>
#include <input/event.h>
#include <input/keycodes.h>
#include <timer_session/connection.h>


namespace Global_keys_handler {
	struct Main;
	using namespace Genode;
}


struct Global_keys_handler::Main
{
	Env &_env;

	Heap _heap { _env.ram(), _env.rm() };

	/**
	 * Configuration buffer
	 */
	Attached_rom_dataspace _config_ds { _env, "config" };

	/**
	 * GUI connection to obtain user input
	 */
	Gui::Connection _nitpicker { _env, "input" };

	/**
	 * Input-event buffer
	 */
	Attached_dataspace _ev_ds { _env.rm(), _nitpicker.input.dataspace() };

	/**
	 * Number of pressed keys, used to distinguish primary keys from key
	 * sequences.
	 */
	unsigned _key_cnt = 0;

	/**
	 * Hover model as reported by nitpicker
	 */
	Constructible<Attached_rom_dataspace> _hover_ds { };

	struct Bool_state
	{
		Registry<Bool_state>::Element _element;

		using Name = String<64>;

		Name const _name;

		bool _state = false;

		Bool_state(Registry<Bool_state> &registry, Xml_node node)
		:
			_element(registry, *this),
			_name(node.attribute_value("name", Name())),
			_state(node.attribute_value("initial", false))
		{ }

		bool enabled() const { return _state; }

		void apply_change(Xml_node event)
		{
			/* modify state of matching name only */
			if (event.attribute_value("bool", Bool_state::Name()) != _name)
				return;

			using Change = String<16>;
			Change const change = event.attribute_value("change", Change());

			if (change == "on")     _state = true;
			if (change == "off")    _state = false;
			if (change == "toggle") _state = !_state;
		}

		bool has_name(Name const &name) const { return name == _name; }
	};

	Registry<Bool_state> _bool_states { };

	struct Report
	{
		Deallocator &_alloc;

		using Name = String<64>;
		Name const _name;

		Registry<Report>::Element _element;

		Registry<Bool_state> const &_bool_states;

		struct Bool_condition
		{
			Registry<Bool_condition>::Element _element;

			Bool_state::Name const _name;

			Bool_condition(Registry<Bool_condition> &registry, Xml_node node)
			:
				_element(registry, *this),
				_name(node.attribute_value("name", Bool_state::Name()))
			{ }

			/**
			 * Return true if boolean condition is true
			 */
			bool satisfied(Registry<Bool_state> const &bool_states) const
			{
				bool result = false;
				bool_states.for_each([&] (Bool_state const &state) {
					if (state.has_name(_name))
						result = state.enabled(); });
				return result;
			}
		};

		struct Hover_condition
		{
			Registry<Hover_condition>::Element _element;

			using Domain = String<160>;

			Domain const _domain;

			Hover_condition(Registry<Hover_condition> &registry, Xml_node node)
			:
				_element(registry, *this),
				_domain(node.attribute_value("domain", Domain()))
			{ }

			/**
			 * Return true if hovered domain equals expected domain
			 */
			bool satisfied(Domain const &hovered) const { return hovered == _domain; }
		};

		Registry<Bool_condition>  _bool_conditions  { };
		Registry<Hover_condition> _hover_conditions { };

		Reporter _reporter;

		bool _initial_report = true;

		bool _curr_value = false;

		void _generate_report()
		{
			Reporter::Xml_generator xml(_reporter, [&] () {
				xml.attribute("enabled", _curr_value ? "yes" : "no"); });
		}

		/*
		 * Handler used for generating delayed reports
		 */
		Constructible<Timer::Connection> _timer { };

		uint64_t const _delay_ms;

		Signal_handler<Report> _timer_handler;

		Report(Env &env, Allocator &alloc,
		       Registry<Report> &reports,
		       Registry<Bool_state> const &bool_states,
		       Xml_node node)
		:
			_alloc(alloc),
			_name(node.attribute_value("name", Name())),
			_element(reports, *this),
			_bool_states(bool_states),
			_reporter(env, _name.string()),
			_delay_ms(node.attribute_value("delay_ms", (uint64_t)0)),
			_timer_handler(env.ep(), *this, &Report::_generate_report)
		{
			_reporter.enabled(true);

			node.for_each_sub_node("bool", [&] (Xml_node bool_node) {
				new (alloc) Bool_condition(_bool_conditions, bool_node); });

			node.for_each_sub_node("hovered", [&] (Xml_node hovered) {
				new (alloc) Hover_condition(_hover_conditions, hovered); });

			if (_delay_ms) {
				_timer.construct(env);
				_timer->sigh(_timer_handler);
			}
		}

		~Report()
		{
			_bool_conditions.for_each([&] (Bool_condition &condition) {
				destroy(_alloc, &condition); });

			_hover_conditions.for_each([&] (Hover_condition &condition) {
				destroy(_alloc, &condition); });
		}

		void update(Hover_condition::Domain const &hovered_domain)
		{
			bool old_value = _curr_value;

			_curr_value = false;

			_bool_conditions.for_each([&] (Bool_condition const &cond) {
				_curr_value |= cond.satisfied(_bool_states); });

			_hover_conditions.for_each([&] (Hover_condition const &cond) {
				_curr_value |= cond.satisfied(hovered_domain); });

			if (!_initial_report && _curr_value == old_value)
				return;

			_initial_report = false;

			if (_delay_ms)
				_timer->trigger_once(_delay_ms*1000);
			else
				_generate_report();
		}

		bool depends_on_hover_info() const
		{
			bool result = false;
			_hover_conditions.for_each([&] (Hover_condition const &) { result = true; });
			return result;
		}
	};

	Registry<Report> _reports { };

	bool _reports_depend_on_hover_info() const
	{
		bool result = false;
		_reports.for_each([&] (Report const &report) {
			result |= report.depends_on_hover_info(); });
		return result;
	}

	void _apply_input_events(unsigned, Input::Event const []);

	/**
	 * Handler that is called on config changes
	 */
	void _handle_config();

	Signal_handler<Main> _config_handler =
		{ _env.ep(), *this, &Main::_handle_config };

	/**
	 * Handler that is called on hover changes or on the arrival of user input
	 */
	void _handle_input();

	Signal_handler<Main> _input_handler = {
		_env.ep(), *this, &Main::_handle_input };

	Main(Env &env) : _env(env)
	{
		_config_ds.sigh(_config_handler);
		_nitpicker.input.sigh(_input_handler);

		_handle_config();
		_handle_input();
	}
};


void Global_keys_handler::Main::_apply_input_events(unsigned num_ev,
                                                    Input::Event const events[])
{
	for (unsigned i = 0; i < num_ev; i++) {

		Input::Event const &ev = events[i];

		if (!ev.press() && !ev.release())
			continue;

		if (ev.press())   _key_cnt++;
		if (ev.release()) _key_cnt--;

		/* ignore key combinations */
		if (_key_cnt > 1) continue;

		using Xml_node = Xml_node;

		auto lambda = [&] (Xml_node node) {

			if (!node.has_type("press") && !node.has_type("release"))
				return;

			/*
			 * XML node applies for current event type, check if the key
			 * matches.
			 */
			using Key_name = String<32>;
			Key_name const expected = node.attribute_value("name", Key_name());

			bool key_matches = false;

			if (node.has_type("press"))
				ev.handle_press([&] (Input::Keycode key, Codepoint) {
					key_matches = (expected == Input::key_name(key)); });

			if (node.has_type("release"))
				ev.handle_release([&] (Input::Keycode key) {
					key_matches = (expected == Input::key_name(key)); });

			if (!key_matches)
				return;

			/*
			 * Manipulate bool state as instructed by the XML node.
			 */
			_bool_states.for_each([&] (Bool_state &state) {
				state.apply_change(node); });
		};

		_config_ds.xml().for_each_sub_node(lambda);
	}
}


void Global_keys_handler::Main::_handle_config()
{
	_config_ds.update();

	Xml_node const config = _config_ds.xml();

	/*
	 * Import bool states
	 */
	_bool_states.for_each([&] (Bool_state &state)
	{
		bool keep_existing_state = false;
		config.for_each_sub_node("bool", [&] (Xml_node node) {
			if (state.has_name(node.attribute_value("name", Bool_state::Name())))
				keep_existing_state = true; });

		if (!keep_existing_state)
			destroy(_heap, &state);
	});

	config.for_each_sub_node("bool", [&] (Xml_node node)
	{
		Bool_state::Name const name = node.attribute_value("name", Bool_state::Name());

		bool state_already_exists = false;
		_bool_states.for_each([&] (Bool_state const &state) {
			if (state.has_name(name))
				state_already_exists = true; });

		if (!state_already_exists)
			new (_heap) Bool_state(_bool_states, node);
	});

	/*
	 * Update report definitions
	 */
	_reports.for_each([&] (Report &report) { destroy(_heap, &report); });

	config.for_each_sub_node("report", [&] (Xml_node report) {
		new (_heap) Report(_env, _heap, _reports, _bool_states, report); });

	/*
	 * Obtain / update hover info if needed
	 */
	if (_reports_depend_on_hover_info() && !_hover_ds.constructed()) {
		_hover_ds.construct(_env, "hover");
		_hover_ds->sigh(_input_handler);
	}

	/*
	 * Trigger initial creation of the reports
	 */
	_handle_input();
}


void Global_keys_handler::Main::_handle_input()
{
	while (unsigned const num_ev = _nitpicker.input.flush())
		_apply_input_events(num_ev, _ev_ds.local_addr<Input::Event const>());

	/* determine currently hovered domain */
	using Domain = Report::Hover_condition::Domain;
	Domain hovered_domain;
	if (_hover_ds.constructed()) {
		_hover_ds->update();
		hovered_domain = _hover_ds->xml().attribute_value("domain", Domain());
	}

	/* re-generate reports */
	_reports.for_each([&] (Report &report) {
		report.update(hovered_domain); });
}


/***************
 ** Component **
 ***************/

void Component::construct(Genode::Env &env) {
	static Global_keys_handler::Main main(env); }
