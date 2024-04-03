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


bool Distant_runtime::apply_runtime_state(Xml_node const &state)
{
	bool reconfiguration_needed = false;
	state.for_each_sub_node("child", [&] (Xml_node const &child) {
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


void Distant_runtime::_handle_hover(Xml_node const &hover)
{
	using Name = Top_level_dialog::Name;
	Name const orig_hovered_dialog = _hovered_dialog;

	_hover_seq_number = { hover.attribute_value("seq_number", 0U) };

	hover.with_sub_node("dialog",
		[&] (Xml_node const &dialog) {
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
			view._with_dialog_hover([&] (Xml_node const &hover) {
				Clicked_at at(*click, hover);
				view._dialog.click(at);
				_click_delivered = true;
				view.refresh();
			});
		});
	}

	if (click.constructed() && clack.constructed()) {
		with_hovered_view(*clack, [&] (View &view) {
			view._with_dialog_hover([&] (Xml_node const &hover) {

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


void Distant_runtime::gen_start_nodes(Xml_generator &xml) const
{
	xml.node("start", [&] {

		xml.attribute("name",    _start_name);
		xml.attribute("version", _version);
		xml.attribute("caps",    _caps.value);

		xml.node("resource", [&] {
			xml.attribute("name", "RAM");
			Number_of_bytes const bytes(_ram.value);
			xml.attribute("quantum", String<64>(bytes)); });

		xml.node("binary", [&] {
			xml.attribute("name", "menu_view"); });

		xml.node("heartbeat", [&] { });

		xml.node("config", [&] {

			xml.node("report", [&] {
				xml.attribute("hover", "yes"); });

			xml.node("libc", [&] {
				xml.attribute("stderr", "/dev/log"); });

			xml.node("vfs", [&] {
				xml.node("tar", [&] {
					xml.attribute("name", "menu_view_styles.tar"); });
				xml.node("dir", [&] {
					xml.attribute("name", "dev");
					xml.node("log", [&] { });
				});
				xml.node("dir", [&] {
					xml.attribute("name", "fonts");
					xml.node("fs", [&] {
						xml.attribute("label", "fonts");
					});
				});
			});

			_views.for_each([&] (View const &view) {
				view._gen_menu_view_dialog(xml); });
		});

		xml.node("route", [&] {
			gen_parent_rom_route(xml, "menu_view");
			gen_parent_rom_route(xml, "ld.lib.so");
			gen_parent_rom_route(xml, "vfs.lib.so");
			gen_parent_rom_route(xml, "libc.lib.so");
			gen_parent_rom_route(xml, "libm.lib.so");
			gen_parent_rom_route(xml, "libpng.lib.so");
			gen_parent_rom_route(xml, "zlib.lib.so");
			gen_parent_rom_route(xml, "menu_view_styles.tar");
			gen_parent_route<Cpu_session>    (xml);
			gen_parent_route<Pd_session>     (xml);
			gen_parent_route<Log_session>    (xml);
			gen_parent_route<Timer::Session> (xml);

			_views.for_each([&] (View const &view) {
				view._gen_menu_view_routes(xml); });

			gen_service_node<Report::Session>(xml, [&] {
				xml.attribute("label", "hover");
				xml.node("parent", [&] {
					xml.attribute("label", "leitzentrale -> runtime_view -> hover");
				});
			});

			gen_service_node<::File_system::Session>(xml, [&] {
				xml.attribute("label", "fonts");
				xml.node("parent", [&] {
					xml.attribute("label", "leitzentrale -> fonts"); }); });
		});
	});
}


void Distant_runtime::View::_gen_menu_view_dialog(Xml_generator &xml) const
{
	xml.node("dialog", [&] {
		xml.attribute("name", name);

		if (min_width)  xml.attribute("width",  min_width);
		if (min_height) xml.attribute("height", min_height);
		if (_opaque)    xml.attribute("opaque", "yes");

		xml.attribute("background", String<20>(_background));
	});
}


void Distant_runtime::View::_gen_menu_view_routes(Xml_generator &xml) const
{
	Session_label::String const label { "leitzentrale -> ", name, "_dialog" };

	gen_service_node<Rom_session>(xml, [&] {
		xml.attribute("label", name);
		xml.node("parent", [&] {
			xml.attribute("label", label); }); });

	gen_service_node<Gui::Session>(xml, [&] {
		xml.attribute("label", name);
		xml.node("parent", [&] {
			xml.attribute("label", label); }); });
}
