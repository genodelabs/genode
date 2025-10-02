/*
 * \brief  Runtime for hosting GUI dialogs in child components
 * \author Norman Feske
 * \date   2023-03-24
 */

/*
 * Copyright (C) 2023 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <dialog/distant_runtime.h>
#include <xml.h>

using namespace Sculpt;
using namespace Dialog;


static bool click(Input::Event const &event)
{
	bool result = false;

	if (event.key_press(Input::BTN_LEFT))
		result = true;

	event.handle_touch([&] (Input::Touch_id id, float, float) {
		if (id.value == 0)
			result = true; });

	return result;
}


static bool clack(Input::Event const &event)
{
	bool result = false;

	if (event.key_release(Input::BTN_LEFT))
		result = true;

	event.handle_touch_release([&] (Input::Touch_id id) {
		if (id.value == 0)
			result = true; });

	return result;
}


bool Distant_runtime::apply_runtime_state(Node const &state)
{
	bool reconfiguration_needed = false;
	state.for_each_sub_node("child", [&] (Node const &child) {
		if (_apply_child_state_report(child))
			reconfiguration_needed = true;
	});

	return reconfiguration_needed;
}


void Distant_runtime::route_input_event(Event::Seq_number seq_number, Input::Event const &event)
{
	_global_seq_number = seq_number;

	if (event.absolute_motion()) _hover_observable_without_click = true;
	if (event.touch())           _hover_observable_without_click = false;

	bool const new_click_seq = !_click_seq_number.constructed()
	                         || _click_seq_number->value != _global_seq_number.value;

	if (click(event) && new_click_seq) {
		_click_seq_number.construct(_global_seq_number);
		_click_delivered = false;
	}

	if (clack(event))
		_clack_seq_number.construct(_global_seq_number);

	if (click(event) || clack(event))
		_try_handle_click_and_clack();
}


void Distant_runtime::_handle_hover(Node const &hover)
{
	using Name = Top_level_dialog::Name;
	Name const orig_hovered_dialog = _hovered_dialog;

	_hover_seq_number = { hover.attribute_value("seq_number", 0U) };

	hover.with_sub_node("dialog",
		[&] (Node const &dialog) {
			_hovered_dialog = dialog.attribute_value("name", Name()); },
		[&] { _hovered_dialog = { }; });

	if (orig_hovered_dialog.valid() && orig_hovered_dialog != _hovered_dialog)
		_views.with_element(orig_hovered_dialog,
			[&] (View &view) { view._leave(); },
			[&] { });

	if (_hovered_dialog.valid())
		_views.with_element(_hovered_dialog,
			[&] (View &view) { view._handle_hover(); },
			[&] { });
}


void Distant_runtime::_try_handle_click_and_clack()
{
	auto with_hovered_view = [&] (Event::Seq_number seq_number, auto const &fn)
	{
		if (_hover_seq_number == seq_number)
			_views.with_element(_hovered_dialog,
			                    [&] (View &view) { fn(view); },
			                    [&] { });
	};

	Constructible<Event::Seq_number> &click = _click_seq_number,
	                                 &clack = _clack_seq_number;

	if (!_click_delivered && click.constructed()) {
		with_hovered_view(*click, [&] (View &view) {
			view._with_dialog_hover([&] (Node const &hover) {
				Clicked_at at(*click, hover);
				view._dialog.click(at);
				_click_delivered = true;
				view.refresh();
			});
		});
	}

	if (click.constructed() && clack.constructed()) {
		with_hovered_view(*clack, [&] (View &view) {
			view._with_dialog_hover([&] (Node const &hover) {

				/*
				 * Deliver stale click if the hover report for the clack
				 * overwrote the intermediate hover report for the click.
				 */
				if (!_click_delivered) {
					Clicked_at at(*click, hover);
					view._dialog.click(at);
					_click_delivered = true;
				}

				/* use click seq number for to associate clack with click */
				Clacked_at at(*click, hover);
				view._dialog.clack(at);
				view.refresh();
			});

			click.destruct();
			clack.destruct();
		});
	}
}


void Distant_runtime::gen_start_nodes(Generator &g) const
{
	g.node("start", [&] {

		g.attribute("name",    _start_name);
		g.attribute("version", _version);
		g.attribute("priority", (int)Priority::LEITZENTRALE);
		g.attribute("caps",    _caps.value);

		auto resource = [&] (auto const &type, auto const &amount)
		{
			g.node("resource", [&] {
				g.attribute("name", type);
				g.attribute("quantum", String<64>(amount)); });
		};

		resource("RAM", Number_of_bytes(_ram.value));
		resource("CPU", 20);

		g.node("binary", [&] {
			g.attribute("name", "menu_view"); });

		g.node("heartbeat", [&] { });

		g.node("config", [&] {

			g.node("report", [&] {
				g.attribute("hover", "yes"); });

			g.node("libc", [&] {
				g.attribute("stderr", "/dev/log"); });

			g.node("vfs", [&] {
				g.node("tar", [&] {
					g.attribute("name", "menu_view_styles.tar"); });
				g.node("dir", [&] {
					g.attribute("name", "dev");
					g.node("log", [&] { });
				});
				g.node("dir", [&] {
					g.attribute("name", "fonts");
					g.node("fs", [&] {
						g.attribute("label", "fonts -> /");
					});
				});
			});

			_views.for_each([&] (View const &view) {
				view._gen_menu_view_dialog(g); });
		});

		g.tabular_node("route", [&] {
			gen_parent_rom_route(g, "menu_view");
			gen_parent_rom_route(g, "ld.lib.so");
			gen_parent_rom_route(g, "vfs.lib.so");
			gen_parent_rom_route(g, "libc.lib.so");
			gen_parent_rom_route(g, "libm.lib.so");
			gen_parent_rom_route(g, "libpng.lib.so");
			gen_parent_rom_route(g, "zlib.lib.so");
			gen_parent_rom_route(g, "menu_view_styles.tar");
			gen_parent_route<Cpu_session>    (g);
			gen_parent_route<Pd_session>     (g);
			gen_parent_route<Log_session>    (g);
			gen_parent_route<Timer::Session> (g);

			_views.for_each([&] (View const &view) {
				view._gen_menu_view_routes(g); });

			gen_service_node<Report::Session>(g, [&] {
				g.attribute("label", "hover");
				g.node("parent", [&] {
					g.attribute("label", "leitzentrale -> runtime_view -> hover");
				});
			});

			gen_service_node<::File_system::Session>(g, [&] {
				g.attribute("label_prefix", "fonts ->");
				g.node("parent", [&] {
					g.attribute("identity", "leitzentrale -> fonts"); }); });
		});
	});
}


void Distant_runtime::View::_gen_menu_view_dialog(Generator &g) const
{
	g.node("dialog", [&] {
		g.attribute("name", name);

		if (min_width)  g.attribute("width",  min_width);
		if (min_height) g.attribute("height", min_height);
		if (_opaque)    g.attribute("opaque", "yes");

		g.attribute("background", String<20>(_background));
	});
}


void Distant_runtime::View::_gen_menu_view_routes(Generator &g) const
{
	Session_label::String const label { "leitzentrale -> ", name, "_dialog" };

	gen_service_node<Rom_session>(g, [&] {
		g.attribute("label", name);
		g.node("parent", [&] {
			g.attribute("label", label); }); });

	gen_service_node<Gui::Session>(g, [&] {
		g.attribute("label", name);
		g.node("parent", [&] {
			g.attribute("label", label); }); });
}
