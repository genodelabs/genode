/*
 * \brief  Keyboard-focus policy
 * \author Norman Feske
 * \date   2018-05-23
 */

/*
 * Copyright (C) 2018 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _KEYBOARD_FOCUS_H_
#define _KEYBOARD_FOCUS_H_

/* Genode includes */
#include <os/reporter.h>

/* local includes */
#include <types.h>
#include <model/popup.h>
#include <model/wpa_passphrase.h>
#include <view/network_widget.h>
#include <view/panel_dialog.h>
#include <view/system_dialog.h>
#include <view/popup_dialog.h>

namespace Sculpt { struct Keyboard_focus; }

struct Sculpt::Keyboard_focus
{
	enum Target { UNDEFINED, WPA_PASSPHRASE, SYSTEM_DIALOG, POPUP, WM } target { UNDEFINED };

	Expanding_reporter _focus_reporter;

	Network_widget      const &_network_widget;
	Wpa_passphrase            &_wpa_passphrase;
	Panel_dialog::State const &_panel;
	System_dialog       const &_system_dialog;
	Popup_dialog        const &_popup_dialog;
	bool                const &_system_visible;
	Popup               const &_popup;

	void update()
	{
		Target const orig_target = target;

		target = UNDEFINED;

		if (_panel.inspect_tab_visible())
			target = WM;

		if ((_popup.state == Popup::VISIBLE) && _popup_dialog.keyboard_needed())
			target = POPUP;

		if (_panel.network_visible() && _network_widget.need_keyboard_focus_for_passphrase())
			target = WPA_PASSPHRASE;

		if (_system_dialog.keyboard_needed() && _system_visible)
			target = SYSTEM_DIALOG;

		if (orig_target == target)
			return;

		if (orig_target == WPA_PASSPHRASE)
			_wpa_passphrase = Wpa_passphrase { };

		_focus_reporter.generate([&] (Xml_generator &xml) {
			switch (target) {

			case WPA_PASSPHRASE:
			case SYSTEM_DIALOG:
			case POPUP:
				xml.attribute("label", "manager -> input");
				break;

			case UNDEFINED:
				break;

			case WM:
				xml.attribute("label", "wm -> ");
				break;
			};
		});
	}

	Keyboard_focus(Env &env,
	               Network_widget      const &network_widget,
	               Wpa_passphrase            &wpa_passphrase,
	               Panel_dialog::State const &panel,
	               System_dialog       const &system_dialog,
	               bool                const &system_visible,
	               Popup_dialog        const &popup_dialog,
	               Popup               const &popup)
	:
		_focus_reporter(env, "focus", "focus"),
		_network_widget(network_widget),
		_wpa_passphrase(wpa_passphrase),
		_panel(panel),
		_system_dialog(system_dialog),
		_popup_dialog(popup_dialog),
		_system_visible(system_visible),
		_popup(popup)
	{
		update();
	}
};

#endif /* _KEYBOARD_FOCUS_H_ */
