/*
 * \brief  Nitpicker main program
 * \author Norman Feske
 * \date   2006-08-04
 */

/*
 * Copyright (C) 2006-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <base/sleep.h>
#include <base/log.h>
#include <base/component.h>
#include <base/attached_rom_dataspace.h>
#include <input/keycodes.h>
#include <root/component.h>
#include <timer_session/connection.h>
#include <input_session/connection.h>
#include <framebuffer_session/connection.h>
#include <os/session_policy.h>
#include <nitpicker_gfx/tff_font.h>
#include <util/dirty_rect.h>

/* local includes */
#include "types.h"
#include "user_state.h"
#include "background.h"
#include "clip_guard.h"
#include "pointer_origin.h"
#include "domain_registry.h"
#include "capture_session.h"
#include "event_session.h"

namespace Nitpicker {
	class  Gui_root;
	class  Capture_root;
	class  Event_root;
	struct Main;
}


/*********************************
 ** Font used for view labeling **
 *********************************/

extern char _binary_default_tff_start[];


/************************************
 ** Framebuffer::Session_component **
 ************************************/

void Framebuffer::Session_component::refresh(int x, int y, int w, int h)
{
	Rect const rect(Point(x, y), Area(w, h));

	_view_stack.mark_session_views_as_dirty(_session, rect);
}


/***************************************
 ** Implementation of the GUI service **
 ***************************************/

class Nitpicker::Gui_root : public Root_component<Gui_session>,
                            public Visibility_controller
{
	private:

		Env                          &_env;
		Attached_rom_dataspace const &_config;
		Session_list                 &_session_list;
		Domain_registry const        &_domain_registry;
		Global_keys                  &_global_keys;
		View_stack                   &_view_stack;
		User_state                   &_user_state;
		View                         &_pointer_origin;
		View                         &_builtin_background;
		Reporter                     &_focus_reporter;
		Focus_updater                &_focus_updater;
		Hover_updater                &_hover_updater;

	protected:

		Gui_session *_create_session(const char *args) override
		{
			Session_label const label = label_from_args(args);

			bool const provides_default_bg = (label == "backdrop");

			Gui_session *session = new (md_alloc())
				Gui_session(_env,
				            session_resources_from_args(args), label,
				            session_diag_from_args(args), _view_stack,
				            _focus_updater, _hover_updater, _pointer_origin,
				            _builtin_background, provides_default_bg,
				            _focus_reporter, *this);

			session->apply_session_policy(_config.xml(), _domain_registry);
			_session_list.insert(session);
			_global_keys.apply_config(_config.xml(), _session_list);
			_focus_updater.update_focus();
			_hover_updater.update_hover();

			return session;
		}

		void _upgrade_session(Gui_session *s, const char *args) override
		{
			s->upgrade(ram_quota_from_args(args));
			s->upgrade(cap_quota_from_args(args));
		}

		void _destroy_session(Gui_session *session) override
		{
			/* invalidate pointers held by other sessions to the destroyed session */
			for (Gui_session *s = _session_list.first(); s; s = s->next())
				s->forget(*session);

			_session_list.remove(session);
			_global_keys.apply_config(_config.xml(), _session_list);

			session->destroy_all_views();
			User_state::Handle_forget_result result = _user_state.forget(*session);

			Genode::destroy(md_alloc(), session);

			if (result.hover_changed)
				_hover_updater.update_hover();

			/* report focus changes */
			if (_focus_reporter.enabled() && result.focus_changed) {
				Reporter::Xml_generator xml(_focus_reporter, [&] () {
					_user_state.report_focused_view_owner(xml, false); });
			}
		}

	public:

		/**
		 * Constructor
		 */
		Gui_root(Env &env,
		         Attached_rom_dataspace const &config,
		         Session_list                 &session_list,
		         Domain_registry        const &domain_registry,
		         Global_keys                  &global_keys,
		         View_stack                   &view_stack,
		         User_state                   &user_state,
		         View                         &pointer_origin,
		         View                         &builtin_background,
		         Allocator                    &md_alloc,
		         Reporter                     &focus_reporter,
		         Focus_updater                &focus_updater,
		         Hover_updater                &hover_updater)
		:
			Root_component<Gui_session>(&env.ep().rpc_ep(), &md_alloc),
			_env(env), _config(config), _session_list(session_list),
			_domain_registry(domain_registry), _global_keys(global_keys),
			_view_stack(view_stack), _user_state(user_state),
			_pointer_origin(pointer_origin),
			_builtin_background(builtin_background),
			_focus_reporter(focus_reporter), _focus_updater(focus_updater),
			_hover_updater(hover_updater)
		{ }


		/*************************************
		 ** Visibility_controller interface **
		 *************************************/

		void _session_visibility(Session_label const &label, Suffix const &suffix,
		                         bool visible)
		{
			Gui::Session::Label const selector(label, suffix);

			for (Gui_session *s = _session_list.first(); s; s = s->next())
				if (s->matches_session_label(selector))
					s->visible(visible);

			_view_stack.update_all_views();
		}

		void hide_matching_sessions(Session_label const &label, Suffix const &suffix) override
		{
			_session_visibility(label, suffix, false);
		}
	
		void show_matching_sessions(Session_label const &label, Suffix const &suffix) override
		{
			_session_visibility(label, suffix, true);
		}
};


/*******************************************
 ** Implementation of the capture service **
 *******************************************/

class Nitpicker::Capture_root : public Root_component<Capture_session>
{
	private:

		using Sessions = Registry<Registered<Capture_session>>;

		Env                      &_env;
		Sessions                  _sessions { };
		View_stack         const &_view_stack;
		Capture_session::Handler &_handler;

	protected:

		Capture_session *_create_session(const char *args) override
		{
			return new (md_alloc())
				Registered<Capture_session>(_sessions, _env,
				                            session_resources_from_args(args),
				                            session_label_from_args(args),
				                            session_diag_from_args(args),
				                            _handler, _view_stack);
		}

		void _upgrade_session(Capture_session *s, const char *args) override
		{
			s->upgrade(ram_quota_from_args(args));
			s->upgrade(cap_quota_from_args(args));
		}

		void _destroy_session(Capture_session *session) override
		{
			Genode::destroy(md_alloc(), session);

			/* shrink screen according to the remaining output back ends */
			_handler.capture_buffer_size_changed();
		}

	public:

		/**
		 * Constructor
		 */
		Capture_root(Env                      &env,
		             Allocator                &md_alloc,
		             View_stack         const &view_stack,
		             Capture_session::Handler &handler)
		:
			Root_component<Capture_session>(&env.ep().rpc_ep(), &md_alloc),
			_env(env), _view_stack(view_stack), _handler(handler)
		{ }

		/**
		 * Determine the size of the bounding box of all capture pixel buffers
		 */
		Area bounding_box() const
		{
			Area result { 0, 0 };
			_sessions.for_each([&] (Capture_session const &session) {
				result = max_area(result, session.buffer_size()); });
			return result;
		}

		/**
		 * Notify all capture clients about the changed screen size
		 */
		void screen_size_changed()
		{
			_sessions.for_each([&] (Capture_session &session) {
				session.screen_size_changed(); });
		}

		void mark_as_damaged(Rect rect)
		{
			_sessions.for_each([&] (Capture_session &session) {
				session.mark_as_damaged(rect); });
		}

		void report_displays(Xml_generator &xml) const
		{
			Area const size = bounding_box();

			if (size.count() == 0)
				return;

			xml.node("display", [&] () {
				xml.attribute("width",  size.w());
				xml.attribute("height", size.h());
			});
		}
};


/*****************************************
 ** Implementation of the event service **
 *****************************************/

class Nitpicker::Event_root : public Root_component<Event_session>
{
	private:

		Env &_env;

		Event_session::Handler &_handler;

	protected:

		Event_session *_create_session(const char *args) override
		{
			return new (md_alloc())
				Event_session(_env,
				              session_resources_from_args(args),
				              session_label_from_args(args),
				              session_diag_from_args(args),
				              _handler);
		}

		void _upgrade_session(Event_session *s, const char *args) override
		{
			s->upgrade(ram_quota_from_args(args));
			s->upgrade(cap_quota_from_args(args));
		}

		void _destroy_session(Event_session *session) override
		{
			Genode::destroy(md_alloc(), session);
		}

	public:

		/**
		 * Constructor
		 */
		Event_root(Env &env, Allocator &md_alloc, Event_session::Handler &handler)
		:
			Root_component<Event_session>(&env.ep().rpc_ep(), &md_alloc),
			_env(env), _handler(handler)
		{ }
};


struct Nitpicker::Main : Focus_updater, Hover_updater,
                         View_stack::Damage,
                         Capture_session::Handler,
                         Event_session::Handler
{
	Env &_env;

	Timer::Connection _timer { _env };

	Signal_handler<Main> _timer_handler = { _env.ep(), *this, &Main::_handle_period };

	unsigned long _timer_period_ms = 10;

	Constructible<Framebuffer::Connection> _framebuffer { };

	struct Input_connection
	{
		Input::Connection connection;

		Attached_dataspace ev_ds;

		Input_connection(Env &env)
		: connection(env), ev_ds(env.rm(), connection.dataspace()) { }
	};

	Constructible<Input_connection> _input { };

	typedef Pixel_rgb888 PT;  /* physical pixel type */

	/*
	 * Initialize framebuffer
	 *
	 * The framebuffer is encapsulated in a volatile object to allow its
	 * reconstruction at runtime as a response to resolution changes.
	 */
	struct Framebuffer_screen
	{
		Framebuffer::Session &framebuffer;

		Framebuffer::Mode const mode = framebuffer.mode();

		Attached_dataspace fb_ds;

		Canvas<PT> screen = { fb_ds.local_addr<PT>(), Point(0, 0), mode.area };

		Area size = screen.size();

		typedef Genode::Dirty_rect<Rect, 3> Dirty_rect;

		Dirty_rect dirty_rect { };

		/**
		 * Constructor
		 */
		Framebuffer_screen(Region_map &rm, Framebuffer::Session &fb)
		:
			framebuffer(fb), fb_ds(rm, framebuffer.dataspace())
		{
			dirty_rect.mark_as_dirty(Rect(Point(0, 0), size));
		}
	};

	bool _request_framebuffer = false;
	bool _request_input       = false;

	Constructible<Framebuffer_screen> _fb_screen { };

	void _handle_fb_mode();
	void _report_displays();

	Signal_handler<Main> _fb_mode_handler = { _env.ep(), *this, &Main::_handle_fb_mode };

	/*
	 * User-input policy
	 */
	Global_keys _global_keys { };

	Session_list _session_list { };

	/*
	 * Construct empty domain registry. The initial version will be replaced
	 * on the first call of 'handle_config'.
	 */
	Heap _domain_registry_heap { _env.ram(), _env.rm() };

	Reconstructible<Domain_registry> _domain_registry {
		_domain_registry_heap, Xml_node("<config/>") };

	Tff_font::Static_glyph_buffer<4096> _glyph_buffer { };

	Tff_font const _font { _binary_default_tff_start, _glyph_buffer };

	Focus      _focus { };
	View_stack _view_stack { _focus, _font, *this };
	User_state _user_state { _focus, _global_keys, _view_stack };

	View_owner _global_view_owner { };

	/*
	 * Create view stack with default elements
	 */
	Pointer_origin _pointer_origin { _global_view_owner };

	Background _builtin_background = { _global_view_owner, Area(99999, 99999) };

	/*
	 * Initialize GUI root interface
	 */
	Sliced_heap _sliced_heap { _env.ram(), _env.rm() };

	Reporter _pointer_reporter  = { _env, "pointer" };
	Reporter _hover_reporter    = { _env, "hover" };
	Reporter _focus_reporter    = { _env, "focus" };
	Reporter _keystate_reporter = { _env, "keystate" };
	Reporter _clicked_reporter  = { _env, "clicked" };
	Reporter _displays_reporter = { _env, "displays" };

	Attached_rom_dataspace _config_rom { _env, "config" };

	Constructible<Attached_rom_dataspace> _focus_rom { };

	Gui_root _gui_root { _env, _config_rom, _session_list, *_domain_registry,
	                     _global_keys, _view_stack, _user_state, _pointer_origin,
	                     _builtin_background, _sliced_heap,
	                     _focus_reporter, *this, *this };

	Capture_root _capture_root { _env, _sliced_heap, _view_stack, *this };

	Event_root _event_root { _env, _sliced_heap, *this };

	void _generate_hover_report()
	{
		if (_hover_reporter.enabled()) {
			Reporter::Xml_generator xml(_hover_reporter, [&] () {
				_user_state.report_hovered_view_owner(xml, false); });
		}
	}

	/**
	 * View_stack::Damage interface
	 */
	void mark_as_damaged(Rect rect) override
	{
		if (_fb_screen.constructed()) {
			_fb_screen->dirty_rect.mark_as_dirty(rect);
		}

		_capture_root.mark_as_damaged(rect);
	}

	void _update_input_connection()
	{
		bool const output_present = (_view_stack.size().count() > 0);
		_input.conditional(_request_input && output_present, _env);
	}

	/**
	 * Capture_session::Handler interface
	 */
	void capture_buffer_size_changed() override
	{
		/*
		 * Determine the new screen size, which is the bounding box of all
		 * present output back ends.
		 */

		Area new_size { 0, 0 };

		if (_fb_screen.constructed())
			new_size = max_area(new_size, _fb_screen->size);

		new_size = max_area(new_size, _capture_root.bounding_box());

		bool const size_changed = (new_size != _view_stack.size());

		if (size_changed) {
			_view_stack.size(new_size);
			_user_state.sanitize_pointer_position();
			_update_pointer_position();
			_capture_root.screen_size_changed();

			/* redraw */
			_view_stack.update_all_views();

			/* notify clients about the change screen mode */
			for (Gui_session *s = _session_list.first(); s; s = s->next())
				s->notify_mode_change();

			_report_displays();
		}

		_update_input_connection();
	}

	/**
	 * Focus_updater interface
	 *
	 * Called whenever a new session appears.
	 */
	void update_focus() override { _handle_focus(); }

	/**
	 * Hover_updater interface
	 *
	 * Called whenever the view composition changes.
	 */
	void update_hover() override
	{
		if (_user_state.update_hover().hover_changed)
			_generate_hover_report();
	}

	/*
	 * Configuration-update handler, executed in the context of the RPC
	 * entrypoint.
	 *
	 * In addition to installing the signal handler, we trigger first signal
	 * manually to turn the initial configuration into effect.
	 */
	void _handle_config();

	Signal_handler<Main> _config_handler = { _env.ep(), *this, &Main::_handle_config };

	/**
	 * Signal handler for externally triggered focus changes
	 */
	void _handle_focus();

	Signal_handler<Main> _focus_handler = { _env.ep(), *this, &Main::_handle_focus };

	/**
	 * Event_session::Handler interface
	 */
	void handle_input_events(User_state::Input_batch) override;

	bool _reported_button_activity = false;
	bool _reported_motion_activity = false;

	unsigned _reported_focus_count = 0;
	unsigned _reported_hover_count = 0;

	unsigned _focus_count = 0;
	unsigned _hover_count = 0;

	void _update_motion_and_focus_activity_reports();

	/**
	 * Signal handler periodically invoked for the reception of user input and redraw
	 */
	void _handle_period();

	Signal_handler<Main> _input_period = { _env.ep(), *this, &Main::_handle_period };

	/**
	 * Counter that is incremented periodically
	 */
	unsigned _period_cnt = 0;

	/**
	 * Period counter when the user was active the last time
	 */
	unsigned _last_button_activity_period = 0,
	         _last_motion_activity_period = 0;

	/**
	 * Number of periods after the last user activity when we regard the user
	 * as becoming inactive
	 */
	unsigned const _activity_threshold = 50;

	/**
	 * True if the user has recently interacted with buttons or keys
	 *
	 * This state is reported as part of focus reports to allow the clipboard
	 * to dynamically adjust its information-flow policy to the user activity.
	 */
	bool _button_activity = false;

	/**
	 * True if the user recently moved the pointer
	 */
	bool _motion_activity = false;

	void _update_pointer_position()
	{
		_view_stack.geometry(_pointer_origin, Rect(_user_state.pointer_pos(), Area()));
	}

	Main(Env &env) : _env(env)
	{
		_view_stack.default_background(_builtin_background);
		_view_stack.stack(_pointer_origin);
		_view_stack.stack(_builtin_background);
		_update_pointer_position();

		_config_rom.sigh(_config_handler);
		_handle_config();

		_timer.sigh(_timer_handler);

		_handle_fb_mode();

		_env.parent().announce(_env.ep().manage(_gui_root));

		if (_config_rom.xml().has_sub_node("capture"))
			_env.parent().announce(_env.ep().manage(_capture_root));

		if (_config_rom.xml().has_sub_node("event"))
			_env.parent().announce(_env.ep().manage(_event_root));

		/*
		 * Detect initial motion activity such that the first hover report
		 * contains the boot-time activity of the user in the very first
		 * report.
		 */
		_handle_period();

		_report_displays();
	}
};


void Nitpicker::Main::handle_input_events(User_state::Input_batch batch)
{
	User_state::Handle_input_result const result =
		_user_state.handle_input_events(batch);

	if (result.button_activity) {
		_last_button_activity_period = _period_cnt;
		_button_activity             = true;
	}

	if (result.motion_activity) {
		_last_motion_activity_period = _period_cnt;
		_motion_activity             = true;
	}

	/*
	 * Report information about currently pressed keys whenever the key state
	 * is affected by the incoming events.
	 */
	if (_keystate_reporter.enabled() && result.key_state_affected) {
		Reporter::Xml_generator xml(_keystate_reporter, [&] () {
			_user_state.report_keystate(xml); });
	}

	/*
	 * Report whenever a non-focused view owner received a click. This report
	 * can be consumed by a focus-managing component.
	 */
	if (_clicked_reporter.enabled() && result.last_clicked_changed) {
		Reporter::Xml_generator xml(_clicked_reporter, [&] () {
			_user_state.report_last_clicked_view_owner(xml); });
	}

	if (result.focus_changed) {
		_focus_count++;
		_view_stack.update_all_views();
	}

	if (result.hover_changed)
		_hover_count++;

	/* report mouse-position updates */
	if (_pointer_reporter.enabled() && result.motion_activity) {
		Reporter::Xml_generator xml(_pointer_reporter, [&] () {
			_user_state.report_pointer_position(xml); });
	}

	/* update pointer position */
	if (result.motion_activity)
		_update_pointer_position();
}


void Nitpicker::Main::_update_motion_and_focus_activity_reports()
{
	/* flag user as inactive after activity threshold is reached */
	if (_period_cnt == _last_button_activity_period + _activity_threshold)
		_button_activity = false;

	if (_period_cnt == _last_motion_activity_period + _activity_threshold)
		_motion_activity = false;

	bool const hover_changed = (_reported_hover_count != _hover_count);
	if (hover_changed || (_reported_motion_activity != _motion_activity)) {
		Reporter::Xml_generator xml(_hover_reporter, [&] () {
			_user_state.report_hovered_view_owner(xml, _motion_activity); });
	}

	bool const focus_changed = (_reported_focus_count != _focus_count);
	if (focus_changed || (_reported_button_activity != _button_activity)) {
		Reporter::Xml_generator xml(_focus_reporter, [&] () {
			_user_state.report_focused_view_owner(xml, _button_activity); });
	}

	_reported_motion_activity = _motion_activity;
	_reported_button_activity = _button_activity;
	_reported_hover_count     = _hover_count;
	_reported_focus_count     = _focus_count;
}


void Nitpicker::Main::_handle_period()
{
	_period_cnt++;

	/* handle batch of pending events */
	if (_input.constructed()) {

		size_t const max_events = _input->ev_ds.size() / sizeof(Input::Event);

		User_state::Input_batch const batch {
			.events = _input->ev_ds.local_addr<Input::Event>(),
			.count  = min(max_events, (size_t)_input->connection.flush()) };

		handle_input_events(batch);
	}

	_update_motion_and_focus_activity_reports();

	/* perform redraw */
	if (_framebuffer.constructed() && _fb_screen.constructed()) {
		/* call 'Dirty_rect::flush' on a copy to preserve the state */
		Dirty_rect dirty_rect = _fb_screen->dirty_rect;
		dirty_rect.flush([&] (Rect const &rect) {
			_view_stack.draw(_fb_screen->screen, rect); });

		/* flush pixels to the framebuffer, reset dirty_rect */
		_fb_screen->dirty_rect.flush([&] (Rect const &rect) {
			_framebuffer->refresh(rect.x1(), rect.y1(),
			                      rect.w(),  rect.h()); });
	}

	/* deliver framebuffer synchronization events */
	for (Gui_session *s = _session_list.first(); s; s = s->next())
		s->submit_sync();
}


/**
 * Helper function for 'handle_config'
 */
static void configure_reporter(Genode::Xml_node config, Genode::Reporter &reporter)
{
	try {
		reporter.enabled(config.sub_node("report")
		                       .attribute_value(reporter.name().string(), false));
	}
	catch (...) { reporter.enabled(false); }
}


void Nitpicker::Main::_handle_focus()
{
	if (!_focus_rom.constructed())
		return;

	_focus_rom->update();

	typedef Gui::Session::Label Label;
	Label const label = _focus_rom->xml().attribute_value("label", Label());

	/*
	 * Determine session that matches the label found in the focus ROM
	 */
	View_owner *next_focus = nullptr;
	for (Gui_session *s = _session_list.first(); s; s = s->next())
		if (s->label() == label)
			next_focus = s;

	if (next_focus)
		_user_state.focus(next_focus->forwarded_focus());
	else
		_user_state.reset_focus();
}


void Nitpicker::Main::_handle_config()
{
	_config_rom.update();

	Xml_node const config = _config_rom.xml();

	/* update global keys policy */
	_global_keys.apply_config(config, _session_list);

	/* update background color */
	_builtin_background.color = Background::default_color();
	if (config.has_sub_node("background"))
		_builtin_background.color =
			config.sub_node("background")
			      .attribute_value("color", Background::default_color());

	configure_reporter(config, _pointer_reporter);
	configure_reporter(config, _hover_reporter);
	configure_reporter(config, _focus_reporter);
	configure_reporter(config, _keystate_reporter);
	configure_reporter(config, _clicked_reporter);
	configure_reporter(config, _displays_reporter);

	/* update domain registry and session policies */
	for (Gui_session *s = _session_list.first(); s; s = s->next())
		s->reset_domain();

	try { _domain_registry.construct(_domain_registry_heap, config); }
	catch (...) { }

	for (Gui_session *s = _session_list.first(); s; s = s->next()) {
		s->apply_session_policy(config, *_domain_registry);
		s->notify_mode_change();
	}

	_view_stack.apply_origin_policy(_pointer_origin);

	/*
	 * Domains may have changed their layering, resort the view stack with the
	 * new constrains.
	 */
	_view_stack.sort_views_by_layer();

	/*
	 * Respond to a configuration change of the input-focus mechanism
	 */
	bool const focus_rom = (config.attribute_value("focus", String<16>()) == "rom");
	if (_focus_rom.constructed() && !focus_rom)
		_focus_rom.destruct();

	if (!_focus_rom.constructed() && focus_rom) {
		_focus_rom.construct(_env, "focus");
		_focus_rom->sigh(_focus_handler);
		_handle_focus();
	}

	/* disable builtin focus handling when using an external focus policy */
	_user_state.focus_via_click(!_focus_rom.constructed());

	/* redraw */
	_view_stack.update_all_views();

	/* update focus report since the domain colors might have changed */
	Reporter::Xml_generator xml(_focus_reporter, [&] () {
		_user_state.report_focused_view_owner(xml, _button_activity); });

	/* update framebuffer output back end */
	bool const request_framebuffer = config.attribute_value("request_framebuffer", false);
	if (request_framebuffer != _request_framebuffer) {
		_request_framebuffer = request_framebuffer;
		_handle_fb_mode();
	}

	/*
	 * Update input back end
	 *
	 * Defer input session creation until at least one capture client
	 * (framebuffer driver) is present.
	 */
	_request_input = config.attribute_value("request_input", false);
	_update_input_connection();
}


void Nitpicker::Main::_report_displays()
{
	if (!_displays_reporter.enabled())
		return;

	Reporter::Xml_generator xml(_displays_reporter, [&] () {
		if (_fb_screen.constructed()) {
			xml.node("display", [&] () {
				xml.attribute("width",  _fb_screen->size.w());
				xml.attribute("height", _fb_screen->size.h());
			});
		}

		_capture_root.report_displays(xml);
	});
}


void Nitpicker::Main::_handle_fb_mode()
{
	if (_request_framebuffer && !_framebuffer.constructed()) {
		_framebuffer.construct(_env, Framebuffer::Mode{});
		_framebuffer->mode_sigh(_fb_mode_handler);
		_framebuffer->sync_sigh(_timer_handler);
		_timer.trigger_periodic(0);
	}

	/* reconstruct '_fb_screen' with updated mode */
	if (_request_framebuffer && _framebuffer.constructed())
		_fb_screen.construct(_env.rm(), *_framebuffer);

	if (!_request_framebuffer && _fb_screen.constructed())
		_fb_screen.destruct();

	if (!_request_framebuffer && _framebuffer.constructed())
		_framebuffer.destruct();

	if (!_request_framebuffer)
		_timer.trigger_periodic(_timer_period_ms*1000);

	capture_buffer_size_changed();
}


void Component::construct(Genode::Env &env)
{
	static Nitpicker::Main nitpicker(env);
}
