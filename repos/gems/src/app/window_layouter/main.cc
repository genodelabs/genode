/*
 * \brief  Window layouter
 * \author Norman Feske
 * \date   2013-02-14
 */

/*
 * Copyright (C) 2013-2018 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <base/log.h>
#include <base/signal.h>
#include <base/component.h>
#include <base/attached_rom_dataspace.h>
#include <base/heap.h>
#include <os/reporter.h>
#include <os/session_policy.h>
#include <gui_session/connection.h>
#include <input_session/client.h>
#include <input/event.h>
#include <input/keycodes.h>
#include <timer_session/connection.h>

/* local includes */
#include <window_list.h>
#include <assign_list.h>
#include <target_list.h>
#include <user_state.h>

namespace Window_layouter { struct Main; }


struct Window_layouter::Main : User_state::Action,
                               Layout_rules::Action,
                               Window_list::Action
{
	Env &_env;

	Attached_rom_dataspace _config { _env, "config" };

	Signal_handler<Main> _config_handler {
		_env.ep(), *this, &Main::_handle_config };

	Timer::Connection _drop_timer { _env };

	Drag _drag { };
	Pick _pick { };

	Signal_handler<Main> _drop_timer_handler {
		_env.ep(), *this, &Main::_handle_drop_timer };

	Heap _heap { _env.ram(), _env.rm() };

	Display_list _display_list { _heap };

	unsigned _to_front_cnt = 1;

	Focus_history _focus_history { };

	Layout_rules _layout_rules { _env, _heap, *this };

	Decorator_margins _decorator_margins { };

	Window_list _window_list {
		_env, _heap, *this, _focus_history, _decorator_margins };

	Assign_list _assign_list { _heap };

	Target_list _target_list { _heap };

	/**
	 * Bring window to front, return true if the stacking order changed
	 */
	bool _to_front(Window &window)
	{
		bool stacking_order_changed = false;

		if (window.to_front_cnt() != _to_front_cnt) {
			_to_front_cnt++;
			window.to_front_cnt(_to_front_cnt);
			stacking_order_changed = true;
		};

		return stacking_order_changed;
	}

	void _update_window_layout()
	{
		_window_list.dissolve_windows_from_assignments();

		_layout_rules.with_rules([&] (Xml_node const &rules) {
			_display_list.update_from_xml(_panorama, rules);
			_assign_list.update_from_xml(rules);
			_target_list.update_from_xml(rules, _display_list);
		});

		_assign_list.assign_windows(_window_list);

		/* position windows */
		_assign_list.for_each([&] (Assign &assign) {
			_target_list.for_each([&] (Target const &target) {

				if (target.name != assign.target_name)
					return;

				assign.for_each_member([&] (Assign::Member &member) {

					member.window.floating(assign.floating());
					member.window.target_area(target.rect.area);

					Rect const rect = assign.window_geometry(member.window.id.value,
					                                         member.window.client_size(),
					                                         target.rect.area,
					                                         _decorator_margins);
					member.window.outer_geometry(rect);
					member.window.maximized(assign.maximized());
				});
			});
		});

		/* bring new windows that solely match a wildcard assignment to the front */
		_assign_list.for_each_wildcard_assigned_window([&] (Window &window) {
			_to_front(window); });

		/* update focus if focused window became invisible */
		if (!visible(_user_state.focused_window_id())) {
			auto window_visible = [&] (Window_id id) { return visible(id); };
			_user_state.focused_window_id(_focus_history.next({}, window_visible));
			_gen_focus();
		}

		_gen_window_layout();

		if (_assign_list.matching_wildcards())
			_gen_rules();

		_gen_resize_request();
	}

	/**
	 * Layout_rules::Action interface
	 */
	void layout_rules_changed() override
	{
		_update_window_layout();

		_gen_resize_request();
	}

	/**
	 * Window_list::Action interface
	 */
	void window_list_changed() override { _update_window_layout(); }

	void _handle_config()
	{
		_config.update();

		Xml_node const &config = _config.xml();

		config.with_optional_sub_node("report", [&] (Xml_node const &report) {
			_rules_reporter.conditional(report.attribute_value("rules", false),
			                            _env, "rules", "rules"); });

		_layout_rules.update_config(config);
	}

	User_state _user_state { *this, _focus_history };

	bool _visible(Window_id const id, auto const &target_cond_fn)
	{
		bool result = false;
		_target_list.for_each([&] (Target const &target) {
			if (target_cond_fn(target))
				_assign_list.for_each_visible(target.name, [&] (Assign const &assign) {
					assign.for_each_member([&] (Assign::Member const &member) {
						if (member.window.id == id)
							result = true; }); }); });
		return result;
	}

	/**
	 * User_state::Action interface
	 */
	bool visible(Window_id const id) override
	{
		return _visible(id, [&] (Target const &target) { return target.visible; });
	}


	/**********************************
	 ** User_state::Action interface **
	 **********************************/

	void close(Window_id id) override
	{
		_window_list.with_window(id, [&] (Window &window) {
			window.close(); });

		_gen_resize_request();
		_gen_focus();
	}

	void to_front(Window_id id) override
	{
		bool stacking_order_changed = false;

		_window_list.with_window(id, [&] (Window &window) {
			stacking_order_changed = _to_front(window); });

		if (stacking_order_changed)
			_gen_rules();
	}

	void focus(Window_id) override
	{
		_gen_window_layout();
		_gen_focus();
	}

	void release_grab() override
	{
		/* wm revokes exclusive input on each focus update */
		_gen_focus();
	}

	void toggle_fullscreen(Window_id id) override
	{
		/* make sure that the specified window is the front-most one */
		to_front(id);

		_window_list.with_window(id, [&] (Window &window) {
			window.maximized(!window.maximized()); });

		_gen_rules();
		_gen_resize_request();
	}

	void _with_target_change(Window_id id, Target::Name to_name, auto const &fn) const
	{
		_target_list.with_target(_assign_list, id, [&] (Target const &from) {
			_target_list.with_target(to_name, [&] (Target const &to) {
				if (&from != &to)
					fn(from, to); }); });
	}

	void _with_retargeted_window(Window_id id, Target const &to, auto const &fn)
	{
		_assign_list.for_each([&] (Assign &assign) {
			Window *window_ptr = nullptr;
			assign.for_each_member([&] (Assign::Member &member) {
				if (member.window.id == id)
					window_ptr = &member.window; });
			if (window_ptr) {
				assign.target_name = to.name;
				fn(*window_ptr);
			}
		});
	}

	void _retarget_window(Window_id id, Target const &to)
	{
		_with_retargeted_window(id, to, [&] (Window &) { });
	}

	void _retarget_and_warp_window(Window_id id, Target const &from, Target const &to)
	{
		_with_retargeted_window(id, to, [&] (Window &window) {
			window.warp(from.rect.at - to.rect.at); });
	}

	void screen(Target::Name const &name) override
	{
		auto with_visible_geometry = [&] (Rect orig, Area target_area, auto const &fn)
		{
			Area const overlap = Rect::intersect(orig, { { }, target_area }).area;
			bool const visible = (overlap.w > 50) && (overlap.h > 50);

			fn(visible ? orig : Rect { { }, orig.area });
		};

		/* change screen under picked window */
		if (_pick.picked)
			_with_target_change(_pick.window_id, name, [&] (Target const &, Target const &to) {
				_with_retargeted_window(_pick.window_id, to, [&] (Window &window) {
					with_visible_geometry(_pick.orig_geometry, to.rect.area, [&] (Rect rect) {
						window.outer_geometry(rect); }); }); });

		/* change of screen under the dragged window */
		if (_drag.dragging())
			_with_target_change(_drag.window_id, name, [&] (Target const &from, Target const &to) {
				if (from.rect == to.rect)
					_retarget_window(_drag.window_id, to); });

		/* repeated activation of screen moves focus to the screen */
		bool already_visible = false;
		_target_list.with_target(name, [&] (Target const &target) {
			already_visible = target.visible; });

		auto visible_on_screen = [&] (Window_id id)
		{
			return _visible(id, [&] (Target const &target) { return target.name == name; });
		};

		if (already_visible && !_drag.dragging()) {
			_user_state.focused_window_id(_focus_history.next({}, visible_on_screen));
			_gen_focus();
		}

		_gen_rules_with_frontmost_screen(name);
	}

	void pick_up(Window_id id) override
	{
		_window_list.with_window(id, [&] (Window const &window) {
			_pick = { .picked        = true,
			          .window_id     = id,
			          .orig_geometry = window.outer_geometry() };
			to_front(id);
		});
	}

	void place_down() override { _pick = { }; }

	void drag(Window_id id, Window::Element element, Point clicked, Point curr) override
	{
		if (_drag.state == Drag::State::SETTLING)
			return;

		bool const moving = _moving(id, element);
		_target_list.with_target_at(curr, [&] (Target const &pointed) {
			_drag = { Drag::State::DRAGGING, moving, id, curr, pointed.rect }; });

		to_front(id);

		bool window_layout_changed = false;

		_window_list.with_window(id, [&] (Window &window) {

			bool const orig_dragged  = window.dragged();
			Rect const orig_geometry = window.effective_inner_geometry();
			window.drag(element, clicked, curr);
			bool const next_dragged  = window.dragged();
			Rect const next_geometry = window.effective_inner_geometry();

			window_layout_changed = orig_geometry.p1() != next_geometry.p1()
			                     || orig_geometry.p2() != next_geometry.p2()
			                     || orig_dragged       != next_dragged;
		});

		if (window_layout_changed)
			_gen_window_layout();

		_gen_resize_request();
	}

	Window::Element free_arrange_element_at(Window_id id, Point const abs_at) override
	{
		using Element = Window::Element;
		Element result { };

		/* window geometry is relative to target */
		Point at { };
		_target_list.with_target(_assign_list, id, [&] (Target const &target) {
			at = abs_at - target.rect.at; });

		_window_list.with_window(id, [&] (Window &window) {
			Rect const rect = window.outer_geometry();
			if (!rect.contains(at))
				return;

			int const x_percent = (100*(at.x - rect.x1()))/rect.w(),
			          y_percent = (100*(at.y - rect.y1()))/rect.h();

			auto with_rel = [&] (int rel, auto const &lo_fn, auto const &mid_fn, auto const &hi_fn)
			{
				if (rel > 75) hi_fn(); else if (rel > 25) mid_fn(); else lo_fn();
			};

			with_rel(x_percent,
				[&] {
					with_rel(y_percent,
						[&] { result = { Element::TOP_LEFT    }; },
						[&] { result = { Element::LEFT        }; },
						[&] { result = { Element::BOTTOM_LEFT }; }); },
				[&] {
					with_rel(y_percent,
						[&] { result = { Element::TOP    }; },
						[&] { result = { Element::TITLE  }; },
						[&] { result = { Element::BOTTOM }; }); },
				[&] {
					with_rel(y_percent,
						[&] { result = { Element::TOP_RIGHT    }; },
						[&] { result = { Element::RIGHT        }; },
						[&] { result = { Element::BOTTOM_RIGHT }; }); }
			);
		});
		return result;
	}

	void free_arrange_hover_changed() override
	{
		_update_window_layout();
	}

	void _handle_drop_timer()
	{
		_drag = { };

		_gen_rules();

		_window_list.for_each_window([&] (Window &window) {
			window.finalize_drag_operation(); });
	}

	bool _moving(Window_id id, Window::Element element)
	{
		if (element.type == Window::Element::TITLE)
			return true;

		/* a non-resizeable window can be moved by dragging its border */
		bool resizeable = false;
		_window_list.with_window(id, [&] (Window const &window) {
			resizeable = window.resizeable(); });
		return !resizeable && element.resize_handle();
	}

	void finalize_drag(Window_id id, Window::Element element, Point, Point curr) override
	{
		/*
		 * Update window layout because highlighting may have changed after the
		 * drag operation. E.g., if the window has not kept up with the
		 * dragging of a resize handle, the resize handle is no longer hovered.
		 *
		 * The call of '_handle_hover' implicitly triggers '_gen_window_layout'.
		 */
		_handle_hover();

		_drag = { };

		/*
		 * Update the target of the assign rule of the dragged window
		 */
		bool const moving = _moving(id, element);
		if (moving) {
			_target_list.with_target_at(curr, [&] (Target const &pointed) {
				_drag = { Drag::State::SETTLING, moving, id, curr, pointed.rect };
				_with_target_change(id, pointed.name, [&] (Target const &from, Target const &to) {
					_retarget_and_warp_window(id, from, to); });
			});
		}

		_drop_timer.trigger_once(250*1000);
	}

	/**
	 * Install handler for responding to hover changes
	 */
	void _handle_hover();

	Signal_handler<Main> _hover_handler {
		_env.ep(), *this, &Main::_handle_hover};

	Attached_rom_dataspace _hover { _env, "hover" };


	void _handle_focus_request();

	Signal_handler<Main> _focus_request_handler {
		_env.ep(), *this, &Main::_handle_focus_request };

	Attached_rom_dataspace _focus_request { _env, "focus_request" };

	int _handled_focus_request_id = 0;


	/**
	 * Respond to decorator-margins information reported by the decorator
	 */
	Attached_rom_dataspace _decorator_margins_rom { _env, "decorator_margins" };

	void _handle_decorator_margins()
	{
		_decorator_margins_rom.update();

		Xml_node const &margins = _decorator_margins_rom.xml();
		margins.with_optional_sub_node("floating", [&] (Xml_node const &floating) {
			_decorator_margins = Decorator_margins::from_xml(floating); });

		/* respond to change by adapting the maximized window geometry */
		_handle_mode_change();
	}

	Signal_handler<Main> _decorator_margins_handler {
		_env.ep(), *this, &Main::_handle_decorator_margins};

	void _handle_input()
	{
		while (_gui.input.pending())
			_user_state.handle_input(_input_ds.local_addr<Input::Event>(),
			                         _gui.input.flush(), _config.xml());
	}

	Signal_handler<Main> _input_handler {
		_env.ep(), *this, &Main::_handle_input };

	Gui::Connection _gui { _env };

	Panorama _panorama { _heap };

	void _handle_mode_change()
	{
		_gui.with_info([&] (Xml_node const &node) {
			_panorama.update_from_xml(node); });

		_update_window_layout();
	}
	
	Signal_handler<Main> _mode_change_handler {
		_env.ep(), *this, &Main::_handle_mode_change };


	Attached_dataspace _input_ds { _env.rm(), _gui.input.dataspace() };

	Expanding_reporter _window_layout_reporter  { _env, "window_layout",  "window_layout"};
	Expanding_reporter _resize_request_reporter { _env, "resize_request", "resize_request"};
	Expanding_reporter _focus_reporter          { _env, "focus",          "focus" };

	Constructible<Expanding_reporter> _rules_reporter { };

	bool _focused_window_maximized() const
	{
		bool result = false;
		Window_id const id = _user_state.focused_window_id();
		_window_list.with_window(id, [&] (Window const &window) {
			result = window.maximized(); });
		return result;
	}

	void _import_window_list(Xml_node const &);
	void _gen_window_layout();
	void _gen_resize_request();
	void _gen_focus();
	void _gen_rules_with_frontmost_screen(Target::Name const &);

	void _gen_rules()
	{
		/* keep order of screens unmodified */
		_gen_rules_with_frontmost_screen(Target::Name());
	}

	void _gen_rules_assignments(Xml_generator &, auto const &);

	/**
	 * Constructor
	 */
	Main(Env &env) : _env(env)
	{
		_gui.info_sigh(_mode_change_handler);
		_handle_mode_change();

		_drop_timer.sigh(_drop_timer_handler);

		_hover.sigh(_hover_handler);
		_decorator_margins_rom.sigh(_decorator_margins_handler);
		_gui.input.sigh(_input_handler);
		_focus_request.sigh(_focus_request_handler);

		_window_list.initial_import();
		_handle_decorator_margins();
		_handle_focus_request();

		/* attach update handler for config */
		_config.sigh(_config_handler);
		_handle_config();

		/* reflect initial rules configuration */
		_gen_rules();
	}
};


void Window_layouter::Main::_gen_window_layout()
{
	/* update hover and focus state of each window */
	_window_list.for_each_window([&] (Window &window) {

		window.focused(window.id == _user_state.focused_window_id());

		bool const hovered = (window.id == _user_state.hover_state().window_id);
		window.hovered(hovered ? _user_state.hover_state().element
		                       : Window::Element { });
	});

	_window_layout_reporter.generate([&] (Xml_generator &xml) {
		_target_list.gen_layout(xml, _assign_list, _drag); });
}


void Window_layouter::Main::_gen_resize_request()
{
	bool resize_needed = false;
	_assign_list.for_each([&] (Assign const &assign) {
		assign.for_each_member([&] (Assign::Member const &member) {
			if (member.window.resize_request_needed())
				resize_needed = true; }); });

	if (!resize_needed)
		return;

	_resize_request_reporter.generate([&] (Xml_generator &xml) {
		_window_list.for_each_window([&] (Window const &window) {
			window.gen_resize_request(xml); }); });

	/* prevent superfluous resize requests for the same size */
	_window_list.for_each_window([&] (Window &window) {
		window.resize_request_updated(); });
}


void Window_layouter::Main::_gen_focus()
{
	_focus_reporter.generate([&] (Xml_generator &xml) {
		xml.node("window", [&] () {
			xml.attribute("id", _user_state.focused_window_id().value); }); });
}


void Window_layouter::Main::_gen_rules_assignments(Xml_generator &xml, auto const &filter_fn)
{
	auto gen_window_geometry = [] (Xml_generator &xml,
	                               Assign const &assign, Window const &window) {
		if (!assign.floating())
			return;

		assign.gen_geometry_attr(xml, { .geometry  = window.effective_inner_geometry(),
		                                .maximized = window.maximized() });
	};

	/* turn wildcard assignments into exact assignments */
	auto fn = [&] (Assign const &assign, Assign::Member const &member) {

		if (!filter_fn(member.window))
			return;

		xml.node("assign", [&] () {
			xml.attribute("label",  member.window.label);
			xml.attribute("target", assign.target_name);
			gen_window_geometry(xml, assign, member.window);
		});
	};
	_assign_list.for_each_wildcard_member(fn);

	/*
	 * Generate existing exact assignments of floating windows,
	 * update attributes according to the current window state.
	 */
	_assign_list.for_each([&] (Assign const &assign) {

		if (assign.wildcard())
			return;

		/*
		 * Determine current geometry of window. If multiple windows
		 * are present with the same label, use the geometry of any of
		 * them as they cannot be distinguished based on their label.
		 */
		bool geometry_generated = false;

		assign.for_each_member([&] (Assign::Member const &member) {

			if (geometry_generated || !filter_fn(member.window))
				return;

			xml.node("assign", [&] () {
				assign.gen_assign_attr(xml);
				gen_window_geometry(xml, assign, member.window);
			});
			geometry_generated = true;
		});
	});
}


void Window_layouter::Main::_gen_rules_with_frontmost_screen(Target::Name const &screen)
{
	if (!_rules_reporter.constructed())
		return;

	_rules_reporter->generate([&] (Xml_generator &xml) {

		_layout_rules.with_rules([&] (Xml_node const &rules) {
			bool display_declared = false;
			rules.for_each_sub_node("display", [&] (Xml_node const &display) {
				display_declared = true;
				copy_node(xml, display); });
			if (display_declared)
				xml.append("\n");
		});

		_target_list.gen_screens(xml, screen);

		/*
		 * Generate exact <assign> nodes for present windows.
		 *
		 * The nodes are generated such that front-most windows appear
		 * before all other windows. The change of the stacking order
		 * is applied when the generated rules are imported the next time.
		 */
		auto front_most = [&] (Window const &window) {
			return (window.to_front_cnt() == _to_front_cnt); };

		auto behind_front = [&] (Window const &window) {
			return !front_most(window); };

		_gen_rules_assignments(xml, front_most);
		_gen_rules_assignments(xml, behind_front);

		/* keep attributes of wildcards and (currently) unused assignments */
		_assign_list.for_each([&] (Assign const &assign) {

			bool no_window_assigned = true;
			assign.for_each_member([&] (Assign::Member const &) {
				no_window_assigned = false; });

			/*
			 * If a window is present that matches the assignment, the <assign>
			 * node was already generated by '_gen_rules_assignments' above.
			 */
			if (assign.wildcard() || no_window_assigned) {

				xml.node("assign", [&] () {
					assign.gen_assign_attr(xml);

					if (assign.floating())
						assign.gen_geometry_attr(xml);
				});
			}
		});
	});
}


void Window_layouter::Main::_handle_hover()
{
	_hover.update();

	User_state::Hover_state const orig_hover_state = _user_state.hover_state();

	_hover.xml().with_sub_node("window",
		[&] (Xml_node const &hover) {
			_user_state.hover({ hover.attribute_value("id", 0U) },
			                  Window::Element::from_xml(hover));
		},
		[&] /* the hover model lacks a window */ {
			_user_state.reset_hover();
		});

	/*
	 * Propagate changed hovering to the decorator
	 *
	 * Should a decorator issue a hover update with the unchanged information,
	 * avoid triggering a superfluous window-layout update. This can happen in
	 * corner cases such as the completion of a window-drag operation.
	 */
	if (_user_state.hover_state() != orig_hover_state)
		_gen_window_layout();
}


void Window_layouter::Main::_handle_focus_request()
{
	_focus_request.update();

	int const id = _focus_request.xml().attribute_value("id", 0);

	/* don't apply the same focus request twice */
	if (id == _handled_focus_request_id)
		return;

	_handled_focus_request_id = id;

	Window::Label const prefix =
		_focus_request.xml().attribute_value("label", Window::Label(""));

	unsigned const next_to_front_cnt = _to_front_cnt + 1;

	bool stacking_order_changed = false;

	auto label_matches = [] (Window::Label const &prefix, Window::Label const &label) {
		return !strcmp(label.string(), prefix.string(), prefix.length() - 1); };

	_window_list.for_each_window([&] (Window &window) {

		if (label_matches(prefix, window.label)) {
			window.to_front_cnt(next_to_front_cnt);
			_user_state.focused_window_id(window.id);
			stacking_order_changed = true;
		}
	});

	if (stacking_order_changed) {
		_to_front_cnt++;
		_gen_focus();
		_gen_rules();
	}
}


void Component::construct(Genode::Env &env)
{
	static Window_layouter::Main application(env);
}
