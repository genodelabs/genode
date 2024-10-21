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
#include <types.h>
#include <user_state.h>
#include <background.h>
#include <clip_guard.h>
#include <pointer_origin.h>
#include <domain_registry.h>
#include <capture_session.h>
#include <event_session.h>

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

void Framebuffer::Session_component::refresh(Rect rect)
{
	_view_stack.mark_session_views_as_dirty(_session, rect);
}


Framebuffer::Session::Blit_result
Framebuffer::Session_component::blit(Blit_batch const &batch)
{
	for (Transfer const &transfer : batch.transfer) {
		if (transfer.valid(_mode)) {
			_buffer_provider.blit(transfer.from, transfer.to);
			Rect const to_rect { transfer.to, transfer.from.area };
			_view_stack.mark_session_views_as_dirty(_session, to_rect);
		}
	}
	return Blit_result::OK;
}


void Framebuffer::Session_component::panning(Point pos)
{
	_buffer_provider.panning(pos);
	_view_stack.mark_session_views_as_dirty(_session, { { 0, 0 }, _mode.area });
}


/***************************************
 ** Implementation of the GUI service **
 ***************************************/

class Nitpicker::Gui_root : public Root_component<Gui_session>
{
	private:

		Env                          &_env;
		Gui_session::Action          &_action;
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

			Genode::Session::Resources resources = session_resources_from_args(args);

			/* account caps for input and framebuffer RPC objects */
			if (resources.cap_quota.value < 2)
				throw Insufficient_cap_quota();
			resources.cap_quota.value -= 2;

			Gui_session *session = new (md_alloc())
				Gui_session(_env, _action,
				            resources, label,
				            session_diag_from_args(args), _view_stack,
				            _focus_updater, _hover_updater, _pointer_origin,
				            _builtin_background, provides_default_bg,
				            _focus_reporter);

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
		         Gui_session::Action          &action,
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
			_env(env), _action(action), _config(config), _session_list(session_list),
			_domain_registry(domain_registry), _global_keys(global_keys),
			_view_stack(view_stack), _user_state(user_state),
			_pointer_origin(pointer_origin),
			_builtin_background(builtin_background),
			_focus_reporter(focus_reporter), _focus_updater(focus_updater),
			_hover_updater(hover_updater)
		{ }
};


/*******************************************
 ** Implementation of the capture service **
 *******************************************/

class Nitpicker::Capture_root : public Root_component<Capture_session>
{
	public:

		struct Action : Interface
		{
			virtual void capture_client_appeared_or_disappeared() = 0;
		};

	private:

		using Sessions = Registry<Registered<Capture_session>>;

		Env                      &_env;
		Action                   &_action;
		Sessions                  _sessions { };
		View_stack         const &_view_stack;
		Capture_session::Handler &_handler;

		Rect _fallback_bounding_box { };

	protected:

		Capture_session *_create_session(const char *args) override
		{
			Capture_session &session = *new (md_alloc())
				Registered<Capture_session>(_sessions, _env,
				                            session_resources_from_args(args),
				                            session_label_from_args(args),
				                            session_diag_from_args(args),
				                            _handler, _view_stack);

			_action.capture_client_appeared_or_disappeared();
			return &session;
		}

		void _upgrade_session(Capture_session *s, const char *args) override
		{
			s->upgrade(ram_quota_from_args(args));
			s->upgrade(cap_quota_from_args(args));
		}

		void _destroy_session(Capture_session *session) override
		{
			/*
			 * Retain buffer size of the last vanishing session. This avoids
			 * mode switches when the only capture client temporarily
			 * disappears (driver restart).
			 */
			_fallback_bounding_box = session->bounding_box();

			Genode::destroy(md_alloc(), session);

			_action.capture_client_appeared_or_disappeared();
		}

	public:

		/**
		 * Constructor
		 */
		Capture_root(Env                      &env,
		             Action                   &action,
		             Allocator                &md_alloc,
		             View_stack         const &view_stack,
		             Capture_session::Handler &handler)
		:
			Root_component<Capture_session>(&env.ep().rpc_ep(), &md_alloc),
			_env(env), _action(action), _view_stack(view_stack), _handler(handler)
		{ }

		void apply_config(Xml_node const &config)
		{
			using Policy = Capture_session::Policy;

			if (config.num_sub_nodes() == 0) {

				/* if no policies are defined, mirror with no constraints */
				_sessions.for_each([&] (Capture_session &session) {
					session.apply_policy(Policy::unconstrained()); });

			} else {

				/* apply constraits per session */
				_sessions.for_each([&] (Capture_session &session) {
					with_matching_policy(session.label(), config,
						[&] (Xml_node const &policy) {
							session.apply_policy(Policy::from_xml(policy));
						},
						[&] { session.apply_policy(Policy::blocked()); }); });
			}
		}

		/**
		 * Determine the bounding box of all capture clients
		 */
		Rect bounding_box() const
		{
			Rect bb { };
			_sessions.for_each([&] (Capture_session const &session) {
				bb = Rect::compound(bb, session.bounding_box()); });

			return bb.valid() ? bb : _fallback_bounding_box;
		}

		/**
		 * Return true if specified position is suited as pointer position
		 */
		bool visible(Pointer const pointer) const
		{
			bool result = false;
			pointer.with_result(
				[&] (Point const p) {
					_sessions.for_each([&] (Capture_session const &session) {
						if (!result && session.bounding_box().contains(p))
							result = true; }); },
				[&] (Nowhere) { });
			return result;
		}

		/**
		 * Return position suitable for the initial pointer position
		 */
		Pointer any_visible_pointer_position() const
		{
			Pointer result = Nowhere { };
			_sessions.for_each([&] (Capture_session const &session) {
				if (session.bounding_box().valid())
					result = session.bounding_box().center({ 1, 1 }); });
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

		void process_damage()
		{
			_sessions.for_each([&] (Capture_session &session) {
				session.process_damage(); });
		}

		void report_panorama(Xml_generator &xml, Rect const domain_panorama) const
		{
			gen_attr(xml, domain_panorama);
			_sessions.for_each([&] (Capture_session const &capture) {
				xml.node("capture", [&] { capture.gen_capture_attr(xml); }); });
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
                         Event_session::Handler,
                         Capture_root::Action,
                         User_state::Action,
                         Gui_session::Action

{
	Env &_env;

	Timer::Connection _timer { _env };

	struct Ticks { uint64_t ms; };

	Ticks _now() { return { .ms = _timer.curr_time().trunc_to_plain_ms().value }; }

	struct Input_connection : Noncopyable
	{
		Env  &_env;
		Main &_main;

		Input::Connection _connection { _env };

		Attached_dataspace _ev_ds { _env.rm(), _connection.dataspace() };

		void _handle()
		{
			size_t const max_events = _ev_ds.size() / sizeof(Input::Event);

			User_state::Input_batch const batch {
				.events = _ev_ds.local_addr<Input::Event>(),
				.count  = min(max_events, (size_t)_connection.flush()) };

			_main.handle_input_events(batch);
		}

		Signal_handler<Input_connection> _handler {
			_env.ep(), *this, &Input_connection::_handle };

		Input_connection(Env &env, Main &main) : _env(env), _main(main)
		{
			_connection.sigh(_handler);
		}

		~Input_connection()
		{
			_connection.sigh(Signal_context_capability());
		}
	};

	Constructible<Input_connection> _input { };

	using PT = Pixel_rgb888;  /* physical pixel type */

	/**
	 * Framebuffer connection used when operating in 'request_framebuffer' mode
	 */
	struct Framebuffer_screen
	{
		Env  &_env;
		Main &_main;

		Framebuffer::Connection _fb { _env, { } };

		Framebuffer::Mode const _mode = _fb.mode();

		Attached_dataspace _fb_ds { _env.rm(), _fb.dataspace() };

		Canvas<PT> _screen { _fb_ds.local_addr<PT>(), Point(0, 0), _mode.area };

		Rect const _rect { { 0, 0 }, _screen.size() };

		using Dirty_rect = Genode::Dirty_rect<Rect, 3>;

		Dirty_rect _dirty_rect { };

		Ticks _previous_sync { };

		Signal_handler<Framebuffer_screen> _sync_handler {
			_env.ep(), *this, &Framebuffer_screen::_handle_sync };

		void _handle_sync()
		{
			/* call 'Dirty_rect::flush' on a copy to preserve the state */
			Dirty_rect dirty_rect = _dirty_rect;
			dirty_rect.flush([&] (Rect const &rect) {
				_main._view_stack.draw(_screen, rect); });

			bool const any_pixels_refreshed = !_dirty_rect.empty();

			/* flush pixels to the framebuffer, reset dirty_rect */
			_dirty_rect.flush([&] (Rect const &rect) {
				_fb.refresh(rect); });

			/* deliver framebuffer synchronization events */
			for (Gui_session *s = _main._session_list.first(); s; s = s->next())
				s->submit_sync();

			if (any_pixels_refreshed)
				_previous_sync = _main._now();
		}

		Framebuffer_screen(Env &env, Main &main) : _env(env), _main(main)
		{
			_fb.mode_sigh(_main._fb_screen_mode_handler);
			_fb.sync_sigh(_sync_handler);
			mark_as_dirty(_rect);
		}

		~Framebuffer_screen()
		{
			_fb.mode_sigh(Signal_context_capability());
			_fb.sync_sigh(Signal_context_capability());
		}

		void mark_as_dirty(Rect rect)
		{
			_dirty_rect.mark_as_dirty(rect);
		}

		void process_damage()
		{
			if (_main._now().ms - _previous_sync.ms > 40)
				_handle_sync();
		}

		bool visible(Point p) const { return _rect.contains(p); }

		Point anywhere() const { return _rect.center({ 1, 1 }); };
	};

	bool _request_framebuffer = false;
	bool _request_input       = false;

	Constructible<Framebuffer_screen> _fb_screen { };

	bool _visible_at_fb_screen(Pointer pointer) const
	{
		return pointer.convert<bool>(
			[&] (Point p) { return _fb_screen.constructed() && _fb_screen->visible(p); },
			[&] (Nowhere) { return false; });
	}

	Pointer _anywhere_at_fb_screen() const
	{
		return _fb_screen.constructed() ? Pointer { _fb_screen->anywhere() }
		                                : Pointer { Nowhere { } };
	}

	Signal_handler<Main> _fb_screen_mode_handler {
		_env.ep(), *this, &Main::_reconstruct_fb_screen };

	void _reconstruct_fb_screen()
	{
		_fb_screen.destruct();

		if (_request_framebuffer)
			_fb_screen.construct(_env, *this);

		capture_buffer_size_changed();
	}

	void _report_panorama();

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
	User_state _user_state { *this, _focus, _global_keys, _view_stack };

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
	Reporter _panorama_reporter = { _env, "panorama" };

	Attached_rom_dataspace _config_rom { _env, "config" };

	Constructible<Attached_rom_dataspace> _focus_rom { };

	Gui_root _gui_root { _env, *this, _config_rom, _session_list, *_domain_registry,
	                     _global_keys, _view_stack, _user_state, _pointer_origin,
	                     _builtin_background, _sliced_heap,
	                     _focus_reporter, *this, *this };

	/**
	 * Gui_session::Action interface
	 */
	void gen_capture_info(Xml_generator &xml, Rect const domain_panorama) const override
	{
		_capture_root.report_panorama(xml, domain_panorama);
	}

	Capture_root _capture_root { _env, *this, _sliced_heap, _view_stack, *this };

	Event_root _event_root { _env, _sliced_heap, *this };

	void _generate_hover_report()
	{
		if (_hover_reporter.enabled()) {
			Reporter::Xml_generator xml(_hover_reporter, [&] () {
				_user_state.report_hovered_view_owner(xml, false); });
		}
	}

	Signal_handler<Main> _damage_handler { _env.ep(), *this, &Main::_handle_damage };

	void _handle_damage()
	{
		if (_fb_screen.constructed())
			_fb_screen->process_damage();

		_capture_root.process_damage();
	}

	/**
	 * View_stack::Damage interface
	 */
	void mark_as_damaged(Rect rect) override
	{
		if (_fb_screen.constructed())
			_fb_screen->mark_as_dirty(rect);

		_capture_root.mark_as_damaged(rect);

		_damage_handler.local_submit();
	}

	void _update_input_connection()
	{
		bool const output_present = (_view_stack.bounding_box().valid());
		_input.conditional(_request_input && output_present, _env, *this);
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

		Rect new_bb { };

		if (_fb_screen.constructed())
			new_bb = Rect::compound(new_bb, Rect { _fb_screen->_rect });

		new_bb = Rect::compound(new_bb, _capture_root.bounding_box());

		bool const size_changed = (new_bb != _view_stack.bounding_box());

		if (size_changed) {
			_view_stack.bounding_box(new_bb);

			if (!_user_state.pointer().ok())
				_user_state.pointer(_capture_root.any_visible_pointer_position());

			_update_pointer_position();
			_capture_root.screen_size_changed();

			/* redraw */
			_view_stack.update_all_views();

			/* notify clients about the change screen mode */
			for (Gui_session *s = _session_list.first(); s; s = s->next())
				s->notify_mode_change();
		}

		_report_panorama();
		_update_input_connection();
	}

	/**
	 * Capture_session::Handler interface
	 */
	void capture_requested(Capture_session::Label const &) override
	{
		/* deliver video-sync events */
		for (Gui_session *s = _session_list.first(); s; s = s->next())
			s->submit_sync();
	}

	/**
	 * User_state::Action interface
	 */
	Pointer sanitized_pointer_position(Pointer const orig_pos, Point pos) override
	{
		if (_capture_root.visible(pos) || _visible_at_fb_screen(pos))
			return pos;

		if (_capture_root.visible(orig_pos) || _visible_at_fb_screen(orig_pos))
			return orig_pos;

		Pointer const captured_pos = _capture_root.any_visible_pointer_position();

		return captured_pos.ok() ? captured_pos : _anywhere_at_fb_screen();
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
	void _apply_capture_config();

	Signal_handler<Main> _config_handler { _env.ep(), *this, &Main::_handle_config };

	/**
	 * Capture_root::Action interface
	 */
	void capture_client_appeared_or_disappeared() override
	{
		_apply_capture_config();
		capture_buffer_size_changed();
	}

	bool _exclusive_input = false;

	/**
	 * Input::Session_component::Action interface
	 */
	void exclusive_input_changed() override
	{
		if (_user_state.exclusive_input() != _exclusive_input) {
			_exclusive_input = _user_state.exclusive_input();

			/* toggle pointer visibility */
			_update_pointer_position();
			_view_stack.update_all_views();
		}
	}

	/**
	 * Signal handler for externally triggered focus changes
	 */
	void _handle_focus();

	Signal_handler<Main> _focus_handler { _env.ep(), *this, &Main::_handle_focus };

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
	 * Track when the user was active the last time
	 */
	Ticks _last_button_activity { },
	      _last_motion_activity { };

	/**
	 * Number of milliseconds since the last user interaction, after which
	 * we regard the user as inactive
	 */
	Ticks const _activity_threshold { .ms = 500 };

	void _update_pointer_position()
	{
		/* move pointer out of the way while a client receives exclusive input */
		if (_user_state.exclusive_input()) {
			_view_stack.geometry(_pointer_origin, Rect { { -1000*1000, 0 }, { } });
			return;
		}
		_user_state.pointer().with_result(
			[&] (Point p) {
				_view_stack.geometry(_pointer_origin, Rect(p, Area{})); },
			[&] (Nowhere) { });
	}

	Main(Env &env) : _env(env)
	{
		_view_stack.default_background(_builtin_background);
		_view_stack.stack(_pointer_origin);
		_view_stack.stack(_builtin_background);
		_update_pointer_position();

		_config_rom.sigh(_config_handler);
		_handle_config();

		_reconstruct_fb_screen();

		_env.parent().announce(_env.ep().manage(_gui_root));

		if (_config_rom.xml().has_sub_node("capture"))
			_env.parent().announce(_env.ep().manage(_capture_root));

		if (_config_rom.xml().has_sub_node("event"))
			_env.parent().announce(_env.ep().manage(_event_root));

		_update_motion_and_focus_activity_reports();

		_report_panorama();
	}
};


void Nitpicker::Main::handle_input_events(User_state::Input_batch batch)
{
	Ticks const now = _now();

	User_state::Handle_input_result const result =
		_user_state.handle_input_events(batch);

	if (result.button_activity) _last_button_activity = now;
	if (result.motion_activity) _last_motion_activity = now;

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

	_update_motion_and_focus_activity_reports();
}


void Nitpicker::Main::_update_motion_and_focus_activity_reports()
{
	Ticks const now = _now();

	bool const button_activity = (now.ms - _last_button_activity.ms < _activity_threshold.ms);
	bool const motion_activity = (now.ms - _last_motion_activity.ms < _activity_threshold.ms);

	bool const hover_changed = (_reported_hover_count != _hover_count);
	if (hover_changed || (_reported_motion_activity != motion_activity)) {
		Reporter::Xml_generator xml(_hover_reporter, [&] {
			_user_state.report_hovered_view_owner(xml, motion_activity); });
	}

	bool const focus_changed = (_reported_focus_count != _focus_count);
	if (focus_changed || (_reported_button_activity != button_activity)) {
		Reporter::Xml_generator xml(_focus_reporter, [&] {
			_user_state.report_focused_view_owner(xml, button_activity); });
	}

	_reported_motion_activity = motion_activity;
	_reported_button_activity = button_activity;
	_reported_hover_count     = _hover_count;
	_reported_focus_count     = _focus_count;
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

	using Label = String<160>;
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


void Nitpicker::Main::_apply_capture_config()
{
	/* propagate capture policies */
	_config_rom.xml().with_optional_sub_node("capture",
		[&] (Xml_node const &capture) {
			_capture_root.apply_config(capture); });
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
	configure_reporter(config, _panorama_reporter);

	capture_client_appeared_or_disappeared();

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
		bool const button_activity = (_now().ms - _last_button_activity.ms < _activity_threshold.ms);
		_user_state.report_focused_view_owner(xml, button_activity); });

	/* update framebuffer output back end */
	bool const request_framebuffer = config.attribute_value("request_framebuffer", false);
	if (request_framebuffer != _request_framebuffer) {
		_request_framebuffer = request_framebuffer;
		_reconstruct_fb_screen();
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


void Nitpicker::Main::_report_panorama()
{
	if (!_panorama_reporter.enabled())
		return;

	Reporter::Xml_generator xml(_panorama_reporter, [&] () {
		if (_fb_screen.constructed())
			xml.node("panorama", [&] { gen_attr(xml, _fb_screen->_rect); });

		_capture_root.report_panorama(xml, _view_stack.bounding_box());
	});
}


void Component::construct(Genode::Env &env)
{
	static Nitpicker::Main nitpicker(env);
}
