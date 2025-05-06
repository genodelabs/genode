/*
 * \brief  Clipboard test
 * \author Norman Feske
 * \date   2015-09-23
 */

/*
 * Copyright (C) 2015-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <base/component.h>
#include <base/attached_rom_dataspace.h>
#include <report_session/connection.h>
#include <os/reporter.h>
#include <util/xml_generator.h>
#include <util/xml_node.h>
#include <timer_session/connection.h>

namespace Test {
	class Nitpicker;
	class Subsystem;
	struct Handle_step_fn;
	class Main;

	using namespace Genode;
}


class Test::Nitpicker
{
	private:

		Env &_env;

		Timer::Session &_timer;

		Expanding_reporter _focus_reporter { _env, "focus" };

		void _focus(char const *domain, bool active)
		{
			_focus_reporter.generate([&] (Xml_generator &xml) {
				xml.attribute("domain", domain);
				xml.attribute("active", active ? "yes" : "no");
			});

			/*
			 * Trigger a state change after a while. We wait a bit after
			 * reporting a new focus to give the new state some time to
			 * propagate through the report-rom server to the clipboard.
			 */
			_timer.trigger_once(250*1000);
		}

	public:

		Nitpicker(Env &env, Timer::Session &timer) : _env(env), _timer(timer) { }

		void focus_active  (char const *domain) { _focus(domain, true);  }
		void focus_inactive(char const *domain) { _focus(domain, false); }
};


/**
 * Callback called each time when a subsystem makes progress
 *
 * This function drives the state machine of the test program.
 */
struct Test::Handle_step_fn : Genode::Interface
{
	virtual void handle_step() = 0;
};


class Test::Subsystem
{
	private:

		/*
		 * Noncopyable
		 */
		Subsystem(Subsystem const &);
		Subsystem &operator = (Subsystem const &);

		Env &_env;

		using Label = String<100>;

		Label _name;

		Handle_step_fn &_handle_step_fn;

		bool _expect_import = true;

		Label _session_label() { return Label(_name, " -> clipboard"); }

		Attached_rom_dataspace _import_rom;

		char const *_import_content = nullptr;

		Report::Connection _export_report { _env, _session_label().string() };

		Attached_dataspace _export_report_ds { _env.rm(), _export_report.dataspace() };

		static void _log_lines(char const *string, size_t len)
		{
			print_lines<200>(string, len,
			                 [&] (char const *line) { log("  ", line); });
		}

		void _handle_import()
		{
			if (!_expect_import) {
				class Unexpected_clipboard_import { };
				throw Unexpected_clipboard_import();
			}

			log("\n", _name, ": import new content:");

			_import_rom.update();

			if (!_import_rom.valid())
				return;

			_import_content = _import_rom.local_addr<char>();
			_log_lines(_import_content, _import_rom.size());

			/* trigger next step */
			_handle_step_fn.handle_step();
		}

		Signal_handler<Subsystem> _import_handler =
			{ _env.ep(), *this, &Subsystem::_handle_import };

		static void _strip_outer_whitespace(char const **str_ptr, size_t &len)
		{
			char const *str = *str_ptr;

			/* strip leading whitespace */
			for (; is_whitespace(*str); str++, len--);

			/* strip trailing whitespace */
			for (; len > 1 && is_whitespace(str[len - 1]); len--);

			*str_ptr = str;
		}

		/**
		 * Return currently present imported text
		 *
		 * \throw Xml_node::Nonexistent_sub_node
		 */
		Xml_node _imported_text() const
		{
			if (!_import_content)
				throw Xml_node::Nonexistent_sub_node();

			Xml_node clipboard(_import_content, _import_rom.size());

			return clipboard.sub_node("text");
		}

	public:

		Subsystem(Env &env, char const *name, Handle_step_fn &handle_step_fn)
		:
			_env(env),
			_name(name),
			_handle_step_fn(handle_step_fn),
			_import_rom(_env, _session_label().string())
		{
			_import_rom.sigh(_import_handler);
		}

		void copy(char const *str)
		{
			Xml_generator::generate(_export_report_ds.bytes(), "clipboard",
				[&] (Xml_generator &xml) {
					xml.attribute("origin", _name.string());
					xml.node("text", [&] { xml.append(str, strlen(str));
				});
			}).with_result(
				[&] (size_t used)  {
					log("\n", _name, ": export content:");
					_log_lines(_export_report_ds.local_addr<char>(), used);
					_export_report.submit(used);
				},
				[&] (Buffer_error) { error("copy exceeded maximum buffer size"); }
			);
		}

		bool has_content(char const *str) const
		{
			using namespace Genode;
			try {
				using String = String<100>;

				String const expected(str);
				String const imported = _imported_text().decoded_content<String>();

				return expected == imported;
			}
			catch (...) { }
			return false;
		}

		bool cleared() const
		{
			try {
				_imported_text();
				return false;
			} catch (...) { }
			return true;
		}

		/**
		 * Configure assertion for situation where no imports are expected
		 */
		void expect_import(bool expect) { _expect_import = expect; }
};


struct Test::Main : Handle_step_fn
{
	Env &_env;

	enum State {
		INIT,
		FOCUSED_HOBBY_DOMAIN,
		EXPECT_CAT_PICTURE,
		FOCUSED_ADMIN_DOMAIN,
		EXPECT_PRIVATE_KEY,
		BLOCKED_REPETITION,
		FOCUSED_WORK_DOMAIN,
		EXPECT_CONTRACT,
		FOCUS_BECOMES_INACTIVE,
		BLOCKED_WHEN_INACTIVE,
		FOCUSED_HOBBY_DOMAIN_AGAIN,
		WAIT_FOR_SUCCESS
	};

	State _state = INIT;

	static char const *_state_name(State state)
	{
		switch (state) {
		case INIT:                       return "INIT";
		case FOCUSED_HOBBY_DOMAIN:       return "FOCUSED_HOBBY_DOMAIN";
		case EXPECT_CAT_PICTURE:         return "EXPECT_CAT_PICTURE";
		case FOCUSED_ADMIN_DOMAIN:       return "FOCUSED_ADMIN_DOMAIN";
		case EXPECT_PRIVATE_KEY:         return "EXPECT_PRIVATE_KEY";
		case BLOCKED_REPETITION:         return "BLOCKED_REPETITION";
		case FOCUSED_WORK_DOMAIN:        return "FOCUSED_WORK_DOMAIN";
		case EXPECT_CONTRACT:            return "EXPECT_CONTRACT";
		case FOCUS_BECOMES_INACTIVE:     return "FOCUS_BECOMES_INACTIVE";
		case BLOCKED_WHEN_INACTIVE:      return "BLOCKED_WHEN_INACTIVE";
		case FOCUSED_HOBBY_DOMAIN_AGAIN: return "FOCUSED_HOBBY_DOMAIN_AGAIN";
		case WAIT_FOR_SUCCESS:           return "WAIT_FOR_SUCCESS";
		}
		return "";
	}

	void _enter_state(State state)
	{
		log("\n-> entering state ", _state_name(state));
		_state = state;
	}

	void handle_step() override
	{
		log("\n -- state ", _state_name(_state), " --");

		char const * const cat_picture         = "cat picture";
		char const * const private_key         = "private key";
		char const * const another_private_key = "another private key";
		char const * const contract            = "contract";
		char const * const garbage             = "garbage";

		char const * const hobby_domain = "hobby";
		char const * const work_domain  = "work";
		char const * const admin_domain = "admin";

		switch (_state) {

		case INIT:
			_nitpicker.focus_active(hobby_domain);
			_enter_state(FOCUSED_HOBBY_DOMAIN);
			return;

		case FOCUSED_HOBBY_DOMAIN:
			_hobby.copy(cat_picture);
			_enter_state(EXPECT_CAT_PICTURE);
			return;

		case EXPECT_CAT_PICTURE:

			if (!_hobby.has_content(cat_picture)
			 || !_work .has_content(cat_picture)
			 || !_admin.has_content(cat_picture))
				return;

			_nitpicker.focus_active(admin_domain);
			_enter_state(FOCUSED_ADMIN_DOMAIN);
			return;

		case FOCUSED_ADMIN_DOMAIN:
			_admin.copy(private_key);
			_enter_state(EXPECT_PRIVATE_KEY);
			return;

		case EXPECT_PRIVATE_KEY:

			if (!_hobby.cleared()
			 || !_work .cleared()
			 || !_admin.has_content(private_key))
				return;

			/*
			 * Issue a copy operation that leaves the hobby and work
			 * domains unchanged. The unchanged domains are not expected
			 * to receive any notification. Otherwise, such notifications
			 * could be misused as a covert channel.
			 */
			_work .expect_import(false);
			_hobby.expect_import(false);
			_admin.copy(another_private_key);

			_timer.trigger_once(500*1000);
			_enter_state(BLOCKED_REPETITION);
			return;

		case BLOCKED_REPETITION:

			/*
			 * Let the work and hobby domains accept new imports.
			 */
			_work .expect_import(true);
			_hobby.expect_import(true);

			_nitpicker.focus_active(work_domain);
			_enter_state(FOCUSED_WORK_DOMAIN);
			return;

		case FOCUSED_WORK_DOMAIN:
			_work.copy(contract);
			_enter_state(EXPECT_CONTRACT);
			return;

		case EXPECT_CONTRACT:

			if (!_hobby.cleared()
			 || !_work .has_content(contract)
			 || !_admin.has_content(contract))
				return;

			_nitpicker.focus_inactive(work_domain);
			_enter_state(FOCUS_BECOMES_INACTIVE);
			return;

		case FOCUS_BECOMES_INACTIVE:

			/*
			 * With the focus becoming inactive, we do not expect the
			 * delivery of any new clipboard content.
			 */
			_work .expect_import(false);
			_admin.expect_import(false);
			_hobby.expect_import(false);
			_work.copy(garbage);

			/*
			 * Since no state changes are triggered from the outside,
			 * we schedule a timeout to proceed.
			 */
			_timer.trigger_once(500*1000);
			_enter_state(BLOCKED_WHEN_INACTIVE);
			return;

		case BLOCKED_WHEN_INACTIVE:
			_nitpicker.focus_active(hobby_domain);
			_enter_state(FOCUSED_HOBBY_DOMAIN_AGAIN);
			return;

		case FOCUSED_HOBBY_DOMAIN_AGAIN:
			/*
			 * Let the work domain try to issue a copy operation while the
			 * hobby domain is focused. The clipboard is expected to block
			 * this report.
			 */
			_work.copy(garbage);
			_timer.trigger_once(500*1000);
			_enter_state(WAIT_FOR_SUCCESS);
			return;

		case WAIT_FOR_SUCCESS:
			break;
		}
	}

	Signal_handler<Main> _step_handler =
		{ _env.ep(), *this, &Main::handle_step };

	Subsystem _admin { _env, "noux",  *this };
	Subsystem _hobby { _env, "linux", *this };
	Subsystem _work  { _env, "win7",  *this };

	Timer::Connection _timer { _env };

	Nitpicker _nitpicker { _env, _timer };

	Main(Env &env) : _env(env)
	{
		_timer.sigh(_step_handler);

		/* trigger first step */
		handle_step();
	}
};


void Component::construct(Genode::Env &env) { static Test::Main main(env); }
