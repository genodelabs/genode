/*
 * \brief  Popup dialog
 * \author Norman Feske
 * \date   2018-09-12
 */

/*
 * Copyright (C) 2018 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _VIEW__POPUP_DIALOG_H_
#define _VIEW__POPUP_DIALOG_H_

#include <types.h>
#include <model/launchers.h>
#include <view/dialog.h>
#include <view/selectable_item.h>

namespace Sculpt { struct Popup_dialog; }


struct Sculpt::Popup_dialog
{
	Env &_env;

	Launchers const &_launchers;

	Runtime_info const &_runtime_info;

	Expanding_reporter _dialog_reporter { _env, "dialog", "popup_dialog" };

	Attached_rom_dataspace _hover_rom { _env, "popup_view_hover" };

	Signal_handler<Popup_dialog> _hover_handler {
		_env.ep(), *this, &Popup_dialog::_handle_hover };

	Hoverable_item _item { };

	bool _hovered = false;

	void _handle_hover()
	{
		_hover_rom.update();

		Xml_node const hover = _hover_rom.xml();

		_hovered = hover.has_sub_node("dialog");

		bool const changed = _item.match(hover, "dialog", "frame", "vbox", "hbox", "name");

		if (changed)
			generate();
	}

	bool hovered() const { return _hovered; };

	void generate()
	{
		_dialog_reporter.generate([&] (Xml_generator &xml) {
			xml.node("frame", [&] () {
				xml.node("vbox", [&] () {

					_launchers.for_each([&] (Launchers::Info const &info) {

						/* allow each launcher to be used only once */
						if (_runtime_info.present_in_runtime(info.path))
							return;

						gen_named_node(xml, "hbox", info.path, [&] () {

							gen_named_node(xml, "float", "left", [&] () {
								xml.attribute("west", "yes");

								xml.node("hbox", [&] () {
									gen_named_node(xml, "button", "button", [&] () {
										_item.gen_button_attr(xml, info.path);
										xml.node("label", [&] () {
											xml.attribute("text", " "); }); });
									gen_named_node(xml, "label", "name", [&] () {
										xml.attribute("text", Path(" ", info.path)); });
								});
							});

							gen_named_node(xml, "hbox", "right", [&] () { });
						});
					});
				});
			});
		});
	}

	struct Action : Interface
	{
		virtual void launch_global(Path const &launcher) = 0;
	};

	void click(Action &action)
	{
		action.launch_global(_item._hovered);
	}

	/*
	 * XXX
	 *
	 * This method should not be needed. However, in some situations,
	 * the popup menu_view does not receive a leave event when the popup
	 * is closed, to the effect of maintaining stale hover information
	 * instead of generating an empty hover report. So from the sculpt-
	 * manager's point of view, the popup is still hovered even if it is
	 * actually closed. The 'reset_hover' shortcuts the regular hover
	 * update in this situation.
	 */
	void reset_hover()
	{
		_item._hovered = Hoverable_item::Id();
		_hovered = false;
	}

	Popup_dialog(Env &env, Launchers const &launchers,
	             Runtime_info const &runtime_info)
	:
		_env(env), _launchers(launchers), _runtime_info(runtime_info)
	{
		_hover_rom.sigh(_hover_handler);

		generate();
	}
};

#endif /* _VIEW__POPUP_DIALOG_H_ */
