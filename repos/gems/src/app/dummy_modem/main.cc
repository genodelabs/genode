/*
 * \brief  Simulation of a modem driver
 * \author Norman Feske
 * \date   2022-06-02
 */

/* Genode includes */
#include <base/component.h>
#include <base/attached_rom_dataspace.h>
#include <os/reporter.h>
#include <timer_session/connection.h>

namespace Dummy_modem {

	using namespace Genode;

	struct Main;
}


struct Dummy_modem::Main
{
	Env &_env;

	Expanding_reporter _reporter { _env, "modem", "state" };

	enum class Power_state { UNKNOWN, OFF, STARTING_UP, ON, SHUTTING_DOWN };

	Power_state _power_state = Power_state::ON;

	unsigned _startup_seconds = 0;
	unsigned _shutdown_seconds = 0;

	struct Pin
	{
		enum class State { REQUIRED, CHECKING, OK, PUK_NEEDED };
		State state = State::OK;
		unsigned check_countdown_seconds = 0;

		enum { INITIAL_REMAINING_ATTEMPTS = 3 };
		unsigned remaining_attempts = INITIAL_REMAINING_ATTEMPTS;

		using Code = String<10>;
		Code current_code { };
		Code failed_code { };

		Pin() { }
	};

	Pin _pin { };

	void _generate_report(Xml_generator &xml) const
	{
		auto power_value = [] (Power_state state)
		{
			switch (state) {
			case Power_state::OFF:           return "off";
			case Power_state::STARTING_UP:   return "starting up";
			case Power_state::ON:            return "on";
			case Power_state::SHUTTING_DOWN: return "shutting down";
			case Power_state::UNKNOWN:       break;
			}
			return "";
		};

		xml.attribute("power", power_value(_power_state));

		if (_power_state == Power_state::STARTING_UP)
			xml.attribute("startup_seconds", _startup_seconds);

		if (_power_state == Power_state::SHUTTING_DOWN)
			xml.attribute("shutdown_seconds", _shutdown_seconds);

		auto pin_value = [] (Pin::State state)
		{
			switch (state) {
			case Pin::State::REQUIRED:   return "required";
			case Pin::State::CHECKING:   return "checking";
			case Pin::State::OK:         return "ok";
			case Pin::State::PUK_NEEDED: return "puk needed";
			}
			return "";
		};

		if (_power_state == Power_state::ON) {
			xml.attribute("pin", pin_value(_pin.state));

			if (_pin.remaining_attempts != Pin::INITIAL_REMAINING_ATTEMPTS)
				xml.attribute("pin_remaining_attempts", _pin.remaining_attempts);
		}

		if (_pin.state == Pin::State::OK) {

			enum Scenario { IDLE, INCOMING_CALL, INITIATED_CALL };

			Scenario scenario = INITIATED_CALL;

			if (scenario == INCOMING_CALL) {
				xml.node("call", [&] {
					xml.attribute("number", "+49123456789");
					xml.attribute("state",  "incoming");
				});
			}

			if (scenario == INITIATED_CALL) {
				xml.node("call", [&] {
					xml.attribute("number", "+4911223344");
					xml.attribute("state",  "outbound");
				});
			}
		}
	}

	void _update_state_report()
	{
		_reporter.generate([&] (Xml_generator &xml) {
			_generate_report(xml); });
	}

	Timer::Connection _timer { _env };

	Signal_handler<Main> _timer_handler {
		_env.ep(), *this, &Main::_handle_timer };

	Attached_rom_dataspace _config { _env, "config" };

	Signal_handler<Main> _config_handler {
		_env.ep(), *this, &Main::_handle_config };

	void _handle_timer()
	{
		/* apply time-driven rules */
		if (_power_state == Power_state::STARTING_UP) {
			_startup_seconds++;
			if (_startup_seconds > 4) {
				_power_state = Power_state::ON;
				_startup_seconds = 0;
			}
		}

		if (_power_state == Power_state::SHUTTING_DOWN) {
			_shutdown_seconds++;
			if (_shutdown_seconds > 5) {
				_power_state = Power_state::OFF;
				_shutdown_seconds = 0;
				_pin = Pin { };
			}
		}

		switch (_pin.state) {

		case Pin::State::REQUIRED:
		case Pin::State::OK:
		case Pin::State::PUK_NEEDED:
			break;

		case Pin::State::CHECKING:
			if (_pin.check_countdown_seconds > 0)
				_pin.check_countdown_seconds--;

			if (_pin.check_countdown_seconds == 0) {
				if (_pin.current_code == "1234") {
					_pin.state = Pin::State::OK;
				} else {
					_pin.state = Pin::State::REQUIRED;
					_pin.failed_code = _pin.current_code;
					if (_pin.remaining_attempts == 0)
						_pin.state = Pin::State::PUK_NEEDED;
					else
						_pin.remaining_attempts--;
				}
			} else {
				_trigger_timer_in_one_second();
			}
			break;
		}

		/* re-apply rules dictated by the configuration */
		_handle_config();
	}

	void _trigger_timer_in_one_second() { _timer.trigger_once(1000*1000UL); }

	void _handle_config()
	{
		_config.update();

		Xml_node const config = _config.xml();
		log("_handle_config: ", config);

		auto power_from_config = [&]
		{
			auto name = config.attribute_value("power", String<10>());

			if (name == "on")  return Power_state::ON;
			if (name == "off") return Power_state::OFF;

			return Power_state::UNKNOWN;
		};

		Power_state const requested_power_state = power_from_config();

		if (requested_power_state != _power_state) {

			if (requested_power_state == Power_state::ON) {
				if (_power_state == Power_state::OFF)
					_power_state =  Power_state::STARTING_UP;
			}

			if (requested_power_state == Power_state::OFF) {
				if (_power_state == Power_state::ON)
					_power_state =  Power_state::SHUTTING_DOWN;
			}

			if (requested_power_state != Power_state::UNKNOWN)
				_trigger_timer_in_one_second();
		}

		switch (_pin.state) {

		case Pin::State::REQUIRED:
			if (config.has_attribute("pin")) {
				Pin::Code const code = config.attribute_value("pin", Pin::Code());
				/* don't re-submit failed pin */
				if (code != _pin.failed_code) {
					_pin.current_code = code;
					_pin.state = Pin::State::CHECKING;
					_pin.check_countdown_seconds = 2;
					_trigger_timer_in_one_second();
				}
			}
			break;

		case Pin::State::CHECKING: /* handled by _handle_timer */
		case Pin::State::OK:
		case Pin::State::PUK_NEEDED:
			break;
		}

		_update_state_report();
	}

	Main(Env &env) : _env(env)
	{
		_timer.sigh(_timer_handler);
		_config.sigh(_config_handler);
		_handle_config();
	}
};


void Component::construct(Genode::Env &env)
{
	static Dummy_modem::Main main(env);
}

