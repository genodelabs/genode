/*
 * \brief  Policy for activating the X-Ray mode
 * \author Norman Feske
 * \date   2015-10-03
 */

/*
 * Copyright (C) 2015 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <os/attached_rom_dataspace.h>
#include <nitpicker_session/connection.h>
#include <os/server.h>
#include <os/config.h>
#include <os/reporter.h>
#include <input/event.h>
#include <input/keycodes.h>
#include <timer_session/connection.h>


namespace Xray_trigger { struct Main; }


struct Xray_trigger::Main
{
	Server::Entrypoint &_ep;

	/**
	 * Nitpicker connection to obtain user input
	 */
	Nitpicker::Connection _nitpicker;

	/**
	 * Input-event buffer
	 */
	Genode::Attached_dataspace _ev_ds { _nitpicker.input()->dataspace() };

	/**
	 * Number of pressed keys, used to distinguish primary keys from key
	 * sequences.
	 */
	unsigned _key_cnt = 0;

	/**
	 * Hover model as reported by nitpicker
	 */
	Genode::Lazy_volatile_object<Genode::Attached_rom_dataspace> _hover_ds;

	/**
	 * Reporter for posting the result of our policy decision
	 */
	Genode::Reporter _xray_reporter { "xray" };

	/**
	 * Timer to delay the xray report
	 */
	Timer::Connection _timer;

	/**
	 * X-Ray criterion depending on key events
	 */
	bool _key_xray = false;

	/**
	 * X-Ray criterion depending on hovered domain
	 */
	bool _hover_xray = false;

	bool _xray() const { return _key_xray || _hover_xray; }

	bool _evaluate_input(bool, unsigned, Input::Event const [], unsigned &) const;
	bool _evaluate_hover(Genode::Xml_node) const;

	/**
	 * Handler that is called on config changes, on hover-model changes, or on
	 * the arrival of user input
	 */
	void _handle_update(unsigned);

	Genode::Signal_rpc_member<Main> _update_dispatcher =
		{ _ep, *this, &Main::_handle_update };

	void _report_xray()
	{
		Genode::Reporter::Xml_generator xml(_xray_reporter, [&] () {
			xml.attribute("enabled", _xray() ? "yes" : "no");
		});
	}

	/**
	 * Handler that is called after the xray report delay
	 */
	void _handle_timeout(unsigned)
	{
		_report_xray();
	}

	Genode::Signal_rpc_member<Main> _timeout_dispatcher =
		{ _ep, *this, &Main::_handle_timeout };

	Main(Server::Entrypoint &ep) : _ep(ep)
	{
		Genode::config()->sigh(_update_dispatcher);

		_timer.sigh(_timeout_dispatcher);

		/* enable xray reporter and produce initial xray report */
		_xray_reporter.enabled(true);
		_report_xray();

		_nitpicker.input()->sigh(_update_dispatcher);
		_handle_update(0);
	}
};


bool Xray_trigger::Main::_evaluate_input(bool key_xray, unsigned num_ev,
                                         Input::Event const events[],
                                         unsigned &key_cnt) const
{
	/* adjust '_key_xray' according to user key input */
	for (unsigned i = 0; i < num_ev; i++) {

		Input::Event const &ev = events[i];

		if (ev.type() != Input::Event::PRESS
		 && ev.type() != Input::Event::RELEASE)
			continue;

		if (ev.type() == Input::Event::PRESS)   key_cnt++;
		if (ev.type() == Input::Event::RELEASE) key_cnt--;

		/* ignore key combinations */
		if (key_cnt > 1) continue;

		typedef Genode::String<32> Key_name;
		Key_name const ev_key_name(Input::key_name(ev.keycode()));

		typedef Genode::Xml_node Xml_node;

		auto lambda = [&] (Xml_node node) {

			if (!node.has_type("press") && !node.has_type("release"))
				return;

			if (node.has_type("press") && ev.type() != Input::Event::PRESS)
				return;

			if (node.has_type("release") && ev.type() != Input::Event::RELEASE)
				return;

			/*
			 * XML node applies for current event type, check if the key
			 * matches.
			 */

			Key_name const cfg_key_name =
				node.attribute_value("name", Key_name());

			if (cfg_key_name != ev_key_name)
				return;

			/*
			 * Manipulate X-Ray mode as instructed by the XML node.
			 */

			if (node.attribute("xray").has_value("on"))
				key_xray = true;

			if (node.attribute("xray").has_value("off"))
				key_xray = false;

			if (node.attribute("xray").has_value("toggle"))
				key_xray = !_key_xray;
		};

		Genode::config()->xml_node().for_each_sub_node(lambda);
	}
	return key_xray;
}


bool Xray_trigger::Main::_evaluate_hover(Genode::Xml_node nitpicker_hover) const
{
	bool hover_xray = false;

	using namespace Genode;

	config()->xml_node().for_each_sub_node("hover", [&] (Xml_node node) {

		typedef String<160> Domain;
		Domain nitpicker_domain = nitpicker_hover.attribute_value("domain", Domain());
		Domain expected_domain  = node.attribute_value("domain", Domain());

		if (nitpicker_domain == expected_domain)
			hover_xray = true;
	});
	return hover_xray;
}


void Xray_trigger::Main::_handle_update(unsigned)
{
	Genode::config()->reload();

	/* remember X-Ray mode prior applying the changes */
	bool const orig_xray = _xray();

	while (unsigned const num_ev = _nitpicker.input()->flush())
		_key_xray = _evaluate_input(_key_xray, num_ev,
		                            _ev_ds.local_addr<Input::Event const>(),
		                            _key_cnt);

	/* obtain / update hover model if needed */
	if (Genode::config()->xml_node().has_sub_node("hover")) {

		if (!_hover_ds.constructed()) {
			_hover_ds.construct("hover");
			_hover_ds->sigh(_update_dispatcher);
		}

		_hover_ds->update();
	}

	try {
		_hover_xray = _evaluate_hover(Genode::Xml_node(_hover_ds->local_addr<char>(),
		                                               _hover_ds->size()));
	} catch (...) { }

	/* generate new X-Ray report if the X-Ray mode changed */
	if (_xray() != orig_xray)
		_timer.trigger_once(125000);
}


namespace Server {

	char const *name() { return "ep"; }

	size_t stack_size() { return 16*1024*sizeof(long); }

	void construct(Entrypoint &ep) { static Xray_trigger::Main main(ep); }
}
