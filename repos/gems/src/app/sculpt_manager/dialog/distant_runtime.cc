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
#include <input/seq_number_generator.h>
#include <xml.h>

using namespace Sculpt;
using namespace Dialog;

static bool click(Input::Event const &event) {
	return Input::Seq_number_generator::click(event); }


static bool clack(Input::Event const &event) {
	return Input::Seq_number_generator::clack(event); }


Progress Distant_runtime::apply_runtime_state(Node const &state)
{
	Progress result = STALLED;
	state.for_each_sub_node("child", [&] (Node const &child) {
		if (_apply_child_state_report(child).progressed)
			result = PROGRESSED; });

	return result;
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


void Distant_runtime::gen_child_nodes(Generator &g) const
{
	g.node("child", [&] {

		gen_child_attr(g, _child_name, Binary_name { "menu_view" },
		               _caps, _ram, Priority::LEITZENTRALE);

		g.attribute("version", _version);

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

			g.tabular([&] {
				_views.for_each([&] (View const &view) {
					view._gen_menu_view_dialog(g); });
			});
		});

		g.tabular_node("connect", [&] {
			connect_parent_rom(g, "vfs.lib.so");
			connect_parent_rom(g, "libc.lib.so");
			connect_parent_rom(g, "libm.lib.so");
			connect_parent_rom(g, "libpng.lib.so");
			connect_parent_rom(g, "zlib.lib.so");
			connect_parent_rom(g, "menu_view_styles.tar");

			_views.for_each([&] (View const &view) {
				view._gen_view_connections(g); });

			gen_named_node(g, "report", "hover", [&] {
				g.node("child", [&] {
					g.attribute("name", "leitzentrale"); }); });

			gen_named_node(g, "fs", "fonts", [&] {
				g.node("child", [&] {
					g.attribute("name", "leitzentrale"); }); });
		});
	});
}


void Distant_runtime::View::_gen_menu_view_dialog(Generator &g) const
{
	g.node("dialog", [&] {
		g.attribute("name", name);

		g.attribute("background", String<20>(_background));

		if (min_width)  g.attribute("width",  min_width);
		if (min_height) g.attribute("height", min_height);
		if (_opaque)    g.attribute("opaque", "yes");
	});
}


void Distant_runtime::View::_gen_view_connections(Generator &g) const
{
	Session_label::String const label { name, "_dialog" };

	gen_named_node(g, "rom", name, [&] {
		g.node("child", [&] { g.attribute("name", "leitzentrale"); }); });

	gen_named_node(g, "gui", name, [&] {
		g.node("child", [&] { g.attribute("name", "leitzentrale"); }); });
}
