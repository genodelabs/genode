/*
 * \brief  Menu-view dialog handling
 * \author Norman Feske
 * \date   2018-05-18
 */

/*
 * Copyright (C) 2018 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <menu_view.h>

using namespace Sculpt;


void Menu_view::_handle_hover()
{
	using Hover_result = Dialog::Hover_result;

	_hover_rom.update();

	bool const orig_hovered = _hovered;

	_hovered = false;

	Hover_result hover_result = Hover_result::UNMODIFIED;

	_hover_rom.xml().with_sub_node("dialog", [&] (Xml_node hover) {
		_hovered = true;
		hover_result = _dialog.hover(hover);
	});

	if (!_hovered)
		_dialog.hover(Xml_node("<empty/>"));

	bool const dialog_hover_changed = (_hovered != orig_hovered),
	           widget_hover_changed = (hover_result == Hover_result::CHANGED);

	if (dialog_hover_changed || widget_hover_changed)
		generate();
}


Menu_view::Menu_view(Env &env, Registry<Child_state> &registry,
                     Dialog &dialog, Start_name const &name,
                     Ram_quota ram_quota, Cap_quota cap_quota,
                     Session_label const &dialog_report_name,
                     Session_label const &hover_rom_name)
:
	_dialog(dialog),
	_child_state(registry, name, ram_quota, cap_quota),
	_dialog_reporter(env, "dialog", dialog_report_name.string()),
	_hover_rom(env, hover_rom_name.string()),
	_hover_handler(env.ep(), *this, &Menu_view::_handle_hover)
{
	_hover_rom.sigh(_hover_handler);

	generate();
}


void Menu_view::generate()
{
	_dialog_reporter.generate([&] (Xml_generator &xml) {
		_dialog.generate(xml); });
}


void Menu_view::reset()
{
	_hovered = false;
	_dialog.hover(Xml_node("<empty/>"));
	_dialog.reset();
}


void Menu_view::gen_start_node(Xml_generator &xml) const
{
	xml.node("start", [&] () {
		_gen_start_node_content(xml); });
}


void Menu_view::_gen_start_node_content(Xml_generator &xml) const
{
	_child_state.gen_start_node_content(xml);

	gen_named_node(xml, "binary", "menu_view");

	xml.node("config", [&] () {
		if (min_width)  xml.attribute("width",  min_width);
		if (min_height) xml.attribute("height", min_height);

		xml.node("libc", [&] () { xml.attribute("stderr", "/dev/log"); });
		xml.node("report", [&] () { xml.attribute("hover", "yes"); });
		xml.node("vfs", [&] () {
			gen_named_node(xml, "tar", "menu_view_styles.tar");

			gen_named_node(xml, "dir", "fonts", [&] () {
				xml.node("fs", [&] () {
					xml.attribute("label", "fonts"); }); });

			gen_named_node(xml, "dir", "dev", [&] () {
				xml.node("log",  [&] () { }); });
		});
	});

	xml.node("route", [&] () {
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

		using Label = String<128>;

		Label const label = _child_state.name();

		gen_service_node<Nitpicker::Session>(xml, [&] () {
			xml.node("parent", [&] () {
				xml.attribute("label", Label("leitzentrale -> ", label)); }); });

		gen_service_node<Rom_session>(xml, [&] () {
			xml.attribute("label", "dialog");
			xml.node("parent", [&] () {
				xml.attribute("label", Label("leitzentrale -> ", label, " -> dialog"));
			});
		});

		gen_service_node<Report::Session>(xml, [&] () {
			xml.attribute("label", "hover");
			xml.node("parent", [&] () {
				xml.attribute("label", Label("leitzentrale -> ", label, " -> hover"));
			});
		});

		gen_service_node<::File_system::Session>(xml, [&] () {
			xml.attribute("label", "fonts");
			xml.node("parent", [&] () {
				xml.attribute("label", "leitzentrale -> fonts"); }); });
	});
}
