/*
 * \brief  Nitpicker main program for Genode
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
#include <base/heap.h>
#include <base/attached_rom_dataspace.h>
#include <input/keycodes.h>
#include <root/component.h>
#include <input_session/connection.h>
#include <framebuffer_session/connection.h>
#include <os/session_policy.h>

/* local includes */
#include "types.h"
#include "user_state.h"
#include "background.h"
#include "clip_guard.h"
#include "pointer_origin.h"
#include "domain_registry.h"

namespace Nitpicker {

	struct Focus_updater { virtual void update_focus() = 0; };

	template <typename> class Root;
	struct Main;
}


/*************************
 ** Font initialization **
 *************************/

extern char _binary_default_tff_start;

namespace Nitpicker {

	Text_painter::Font default_font(&_binary_default_tff_start);
}


/************************************
 ** Framebuffer::Session_component **
 ************************************/

void Framebuffer::Session_component::refresh(int x, int y, int w, int h)
{
	Rect const rect(Point(x, y), Area(w, h));

	_view_stack.mark_session_views_as_dirty(_session, rect);
}


/*****************************************
 ** Implementation of Nitpicker service **
 *****************************************/

template <typename PT>
class Nitpicker::Root : public Root_component<Session_component>,
                        public Visibility_controller
{
	private:

		Env                          &_env;
		Attached_rom_dataspace const &_config;
		Session_list                 &_session_list;
		Domain_registry const        &_domain_registry;
		Global_keys                  &_global_keys;
		Framebuffer::Mode             _scr_mode;
		View_stack                   &_view_stack;
		User_state                   &_user_state;
		View_component               &_pointer_origin;
		View_component               &_builtin_background;
		Framebuffer::Session         &_framebuffer;
		Reporter                     &_focus_reporter;
		Focus_updater                &_focus_updater;

	protected:

		Session_component *_create_session(const char *args)
		{
			size_t const ram_quota = Arg_string::find_arg(args, "ram_quota").ulong_value(0);

			size_t const required_quota = Input::Session_component::ev_ds_size()
			                            + align_addr(sizeof(Session::Command_buffer), 12);

			if (ram_quota < required_quota) {
				warning("Insufficient dontated ram_quota (", ram_quota,
				        " bytes), require ", required_quota, " bytes");
				throw Insufficient_ram_quota();
			}

			size_t const unused_quota = ram_quota - required_quota;

			Session_label const label = label_from_args(args);
			bool const provides_default_bg = (label == "backdrop");

			Session_component *session = new (md_alloc())
				Session_component(_env, label, _view_stack, _user_state,
				                  _pointer_origin, _builtin_background, _framebuffer,
				                  provides_default_bg, *md_alloc(), unused_quota,
				                  _focus_reporter, *this);

			session->apply_session_policy(_config.xml(), _domain_registry);
			_session_list.insert(session);
			_global_keys.apply_config(_config.xml(), _session_list);
			_focus_updater.update_focus();

			return session;
		}

		void _upgrade_session(Session_component *s, const char *args)
		{
			size_t ram_quota = Arg_string::find_arg(args, "ram_quota").ulong_value(0);
			s->upgrade_ram_quota(ram_quota);
		}

		void _destroy_session(Session_component *session)
		{
			_session_list.remove(session);
			_global_keys.apply_config(_config.xml(), _session_list);

			session->destroy_all_views();
			_user_state.forget(*session);

			Genode::destroy(md_alloc(), session);
		}

	public:

		/**
		 * Constructor
		 */
		Root(Env &env, Attached_rom_dataspace const &config,
		     Session_list &session_list, Domain_registry const &domain_registry,
		     Global_keys &global_keys, View_stack &view_stack,
		     User_state &user_state, View_component &pointer_origin,
		     View_component &builtin_background, Allocator &md_alloc,
		     Framebuffer::Session &framebuffer, Reporter &focus_reporter,
		     Focus_updater &focus_updater)
		:
			Root_component<Session_component>(&env.ep().rpc_ep(), &md_alloc),
			_env(env), _config(config), _session_list(session_list),
			_domain_registry(domain_registry), _global_keys(global_keys),
			_view_stack(view_stack), _user_state(user_state),
			_pointer_origin(pointer_origin),
			_builtin_background(builtin_background),
			_framebuffer(framebuffer),
			_focus_reporter(focus_reporter), _focus_updater(focus_updater)
		{ }


		/*************************************
		 ** Visibility_controller interface **
		 *************************************/

		void _session_visibility(Session_label const &label, Suffix const &suffix,
		                         bool visible)
		{
			Nitpicker::Session::Label const selector(label, suffix);

			for (Session_component *s = _session_list.first(); s; s = s->next())
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


struct Nitpicker::Main : Focus_updater
{
	Env &_env;

	Framebuffer::Connection _framebuffer { _env, Framebuffer::Mode() };

	Input::Connection _input { _env };

	Attached_dataspace _ev_ds { _env.rm(), _input.dataspace() };

	typedef Pixel_rgb565 PT;  /* physical pixel type */

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

		Canvas<PT> screen = { fb_ds.local_addr<PT>(), Area(mode.width(), mode.height()) };

		Area size = screen.size();

		/**
		 * Constructor
		 */
		Framebuffer_screen(Region_map &rm, Framebuffer::Session &fb)
		: framebuffer(fb), fb_ds(rm, framebuffer.dataspace()) { }
	};

	Reconstructible<Framebuffer_screen> _fb_screen = { _env.rm(), _framebuffer };

	void _handle_fb_mode();

	Signal_handler<Main> _fb_mode_handler = { _env.ep(), *this, &Main::_handle_fb_mode };

	/*
	 * User-input policy
	 */
	Global_keys _global_keys;

	Session_list _session_list;

	/*
	 * Construct empty domain registry. The initial version will be replaced
	 * on the first call of 'handle_config'.
	 */
	Heap _domain_registry_heap { _env.ram(), _env.rm() };

	Reconstructible<Domain_registry> _domain_registry {
		_domain_registry_heap, Xml_node("<config/>") };

	Focus      _focus;
	View_stack _view_stack { _fb_screen->screen.size(), _focus };
	User_state _user_state { _focus, _global_keys, _view_stack };

	View_owner _global_view_owner;

	/*
	 * Create view stack with default elements
	 */
	Pointer_origin _pointer_origin { _global_view_owner };

	Background _builtin_background = { _global_view_owner, Area(99999, 99999) };

	/*
	 * Initialize Nitpicker root interface
	 */
	Sliced_heap _sliced_heap { _env.ram(), _env.rm() };

	Reporter _pointer_reporter  = { _env, "pointer" };
	Reporter _hover_reporter    = { _env, "hover" };
	Reporter _focus_reporter    = { _env, "focus" };
	Reporter _keystate_reporter = { _env, "keystate" };
	Reporter _clicked_reporter  = { _env, "clicked" };

	Attached_rom_dataspace _config_rom { _env, "config" };

	Constructible<Attached_rom_dataspace> _focus_rom;

	Root<PT> _root = { _env, _config_rom, _session_list, *_domain_registry,
	                   _global_keys, _view_stack, _user_state, _pointer_origin,
	                   _builtin_background, _sliced_heap, _framebuffer,
	                   _focus_reporter, *this };

	/**
	 * Focus_updater interface
	 *
	 * Called whenever a new session appears.
	 */
	void update_focus() override { _handle_focus(); }

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
	 * Signal handler invoked on the reception of user input
	 */
	void _handle_input();

	Signal_handler<Main> _input_handler = { _env.ep(), *this, &Main::_handle_input };

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
	unsigned _activity_threshold = 50;

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

	/**
	 * Perform redraw and flush pixels to the framebuffer
	 */
	void _draw_and_flush()
	{
		_view_stack.draw(_fb_screen->screen).flush([&] (Rect const &rect) {
			_framebuffer.refresh(rect.x1(), rect.y1(),
			                     rect.w(),  rect.h()); });
	}

	Main(Env &env) : _env(env)
	{
		_view_stack.default_background(_builtin_background);
		_view_stack.stack(_pointer_origin);
		_view_stack.stack(_builtin_background);

		_config_rom.sigh(_config_handler);
		_handle_config();

		_framebuffer.sync_sigh(_input_handler);
		_framebuffer.mode_sigh(_fb_mode_handler);

		_env.parent().announce(_env.ep().manage(_root));
	}
};


void Nitpicker::Main::_handle_input()
{
	_period_cnt++;

	bool const old_button_activity = _button_activity;
	bool const old_motion_activity = _motion_activity;

	/* handle batch of pending events */
	User_state::Handle_input_result const result =
		_user_state.handle_input_events(_ev_ds.local_addr<Input::Event>(),
		                                _input.flush());

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

	if (result.focus_changed)
		_view_stack.update_all_views();

	/* flag user as inactive after activity threshold is reached */
	if (_period_cnt == _last_button_activity_period + _activity_threshold)
		_button_activity = false;

	if (_period_cnt == _last_motion_activity_period + _activity_threshold)
		_motion_activity = false;

	/* report mouse-position updates */
	if (_pointer_reporter.enabled() && result.motion_activity) {
		Reporter::Xml_generator xml(_pointer_reporter, [&] () {
			_user_state.report_pointer_position(xml); });
	}

	/* report hover changes */
	if (_hover_reporter.enabled() && !result.key_pressed
	 && (result.hover_changed || (old_motion_activity != _motion_activity))) {
		Reporter::Xml_generator xml(_hover_reporter, [&] () {
			_user_state.report_hovered_view_owner(xml, _motion_activity); });
	}

	/* report focus changes */
	if (result.focus_changed || (old_button_activity != _button_activity)) {
		Reporter::Xml_generator xml(_focus_reporter, [&] () {
			_user_state.report_focused_view_owner(xml, _button_activity); });
	}

	/* update pointer position */
	if (result.motion_activity)
		_view_stack.geometry(_pointer_origin, Rect(_user_state.pointer_pos(), Area()));

	/* perform redraw and flush pixels to the framebuffer */
	_view_stack.draw(_fb_screen->screen).flush([&] (Rect const &rect) {
		_framebuffer.refresh(rect.x1(), rect.y1(),
		                     rect.w(),  rect.h()); });

	_view_stack.mark_all_views_as_clean();

	/* deliver framebuffer synchronization events */
	for (Session_component *s = _session_list.first(); s; s = s->next())
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

	typedef Session::Label Label;
	Label const label = _focus_rom->xml().attribute_value("label", Label());

	/* determine session that matches the label found in the focus ROM */
	View_owner *next_focus = nullptr;
	for (Session_component *s = _session_list.first(); s; s = s->next())
		if (s->label() == label)
			next_focus = s;

	if (next_focus)
		_user_state.focus(*next_focus);
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

	/* update domain registry and session policies */
	for (Session_component *s = _session_list.first(); s; s = s->next())
		s->reset_domain();

	try { _domain_registry.construct(_domain_registry_heap, config); }
	catch (...) { }

	for (Session_component *s = _session_list.first(); s; s = s->next())
		s->apply_session_policy(config, *_domain_registry);

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
}


void Nitpicker::Main::_handle_fb_mode()
{
	/* reconstruct framebuffer screen and menu bar */
	_fb_screen.construct(_env.rm(), _framebuffer);

	/* let the view stack use the new size */
	_view_stack.size(Area(_fb_screen->mode.width(), _fb_screen->mode.height()));

	/* redraw */
	_view_stack.update_all_views();

	/* notify clients about the change screen mode */
	for (Session_component *s = _session_list.first(); s; s = s->next())
		s->notify_mode_change();
}


void Component::construct(Genode::Env &env)
{
	/* XXX execute constructors of global statics */
	env.exec_static_constructors();

	static Nitpicker::Main nitpicker(env);
}
