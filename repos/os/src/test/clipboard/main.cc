/*
 * \brief  Clipboard test
 * \author Norman Feske
 * \date   2015-09-23
 */

/*
 * Copyright (C) 2015 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <os/server.h>
#include <report_session/connection.h>
#include <os/attached_rom_dataspace.h>
#include <os/reporter.h>
#include <util/xml_generator.h>
#include <util/xml_node.h>
#include <timer_session/connection.h>


class Nitpicker
{
	private:

		Timer::Session &_timer;

		Genode::Reporter _focus_reporter { "focus" };

		void _focus(char const *domain, bool active)
		{
			Genode::Reporter::Xml_generator xml(_focus_reporter, [&] () {
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

		Nitpicker(Timer::Session &timer)
		:
			_timer(timer)
		{
			_focus_reporter.enabled(true);
		}

		void focus_active  (char const *domain) { _focus(domain, true);  }
		void focus_inactive(char const *domain) { _focus(domain, false); }
};


/**
 * Callback called each time when a subsystem makes progress
 *
 * This function drives the state machine of the test program.
 */
struct Handle_step_fn
{
	virtual void handle_step(unsigned) = 0;
};


class Subsystem
{
	private:

		Server::Entrypoint &_ep;

		typedef Genode::String<100> Label;

		Label _name;

		Handle_step_fn &_handle_step_fn;

		bool _expect_import = true;

		Label _session_label()
		{
			char buf[Label::capacity()];
			Genode::snprintf(buf, sizeof(buf), "%s -> clipboard", _name.string());
			return Label(Genode::Cstring(buf));
		}

		Genode::Attached_rom_dataspace _import_rom;

		char const *_import_content = nullptr;

		Report::Connection _export_report { _session_label().string() };

		Genode::Attached_dataspace _export_report_ds { _export_report.dataspace() };

		static void _log_lines(char const *string, Genode::size_t len)
		{
			Genode::print_lines<200>(string, len,
			                         [&] (char const *line) { Genode::log("  ", line); });
		}

		void _handle_import(unsigned)
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
			_handle_step_fn.handle_step(0);
		}

		Genode::Signal_rpc_member<Subsystem> _import_dispatcher =
			{ _ep, *this, &Subsystem::_handle_import };

		static void _strip_outer_whitespace(char const **str_ptr, Genode::size_t &len)
		{
			char const *str = *str_ptr;

			/* strip leading whitespace */
			for (; Genode::is_whitespace(*str); str++, len--);

			/* strip trailing whitespace */
			for (; len > 1 && Genode::is_whitespace(str[len - 1]); len--);

			*str_ptr = str;
		}

		/**
		 * Return currently present imported text
		 *
		 * \throw Xml_node::Nonexistent_sub_node
		 */
		Genode::Xml_node _imported_text() const
		{
			if (!_import_content)
				throw Genode::Xml_node::Nonexistent_sub_node();

			Genode::Xml_node clipboard(_import_content,
			                           _import_rom.size());

			return clipboard.sub_node("text");
		}

	public:

		Subsystem(Server::Entrypoint &ep, char const *name,
		          Handle_step_fn &handle_step_fn)
		:
			_ep(ep),
			_name(name),
			_handle_step_fn(handle_step_fn),
			_import_rom(_session_label().string())
		{
			_import_rom.sigh(_import_dispatcher);
		}

		void copy(char const *str)
		{
			Genode::Xml_generator xml(_export_report_ds.local_addr<char>(),
			                          _export_report_ds.size(),
			                          "clipboard", [&] ()
			{
				xml.attribute("origin", _name.string());
				xml.node("text", [&] () {
					xml.append(str, Genode::strlen(str));
				});
			});

			log("\n", _name, ": export content:");
			_log_lines(_export_report_ds.local_addr<char>(), xml.used());

			_export_report.submit(xml.used());
		}

		bool has_content(char const *str) const
		{
			using namespace Genode;
			try {
				typedef Genode::String<100> String;

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


namespace Server { struct Main; }


struct Server::Main : Handle_step_fn
{
	Entrypoint &_ep;

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

	void handle_step(unsigned cnt) override
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

	Genode::Signal_rpc_member<Main> _step_dispatcher =
		{ _ep, *this, &Main::handle_step };

	Subsystem _admin { _ep, "noux",  *this };
	Subsystem _hobby { _ep, "linux", *this };
	Subsystem _work  { _ep, "win7",  *this };

	Timer::Connection _timer;

	Nitpicker _nitpicker { _timer };

	Main(Entrypoint &ep) : _ep(ep)
	{
		_timer.sigh(_step_dispatcher);

		/* trigger first step */
		handle_step(0);
	}
};


namespace Server {

	char const *name() { return "ep"; }

	size_t stack_size() { return 4*1024*sizeof(long); }

	void construct(Entrypoint &ep)
	{
		static Main main(ep);
	}
}
