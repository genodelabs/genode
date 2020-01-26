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
#include <view/network_dialog.h>
#include <view/panel_dialog.h>

namespace Sculpt { struct Keyboard_focus; }

struct Sculpt::Keyboard_focus
{
	enum Target { INITIAL, WPA_PASSPHRASE, WM } target { INITIAL };

	Expanding_reporter _focus_reporter;

	Network_dialog      const &_network_dialog;
	Wpa_passphrase            &_wpa_passphrase;
	Panel_dialog::State const &_panel;

	void update()
	{
		Target const orig_target = target;

		target = WM;

		if (_panel.network_visible() && _network_dialog.need_keyboard_focus_for_passphrase())
			target = WPA_PASSPHRASE;

		if (orig_target == target)
			return;

		if (orig_target == WPA_PASSPHRASE)
			_wpa_passphrase = Wpa_passphrase { };

		_focus_reporter.generate([&] (Xml_generator &xml) {
			switch (target) {

			case WPA_PASSPHRASE:
				xml.attribute("label", "manager -> input");
				break;

			case INITIAL:
			case WM:
				xml.attribute("label", "wm -> ");
				break;
			};
		});
	}

	Keyboard_focus(Env &env,
	               Network_dialog      const &network_dialog,
	               Wpa_passphrase            &wpa_passphrase,
	               Panel_dialog::State const &panel)
	:
		_focus_reporter(env, "focus", "focus"),
		_network_dialog(network_dialog),
		_wpa_passphrase(wpa_passphrase),
		_panel(panel)
	{
		update();
	}
};

#endif /* _KEYBOARD_FOCUS_H_ */
