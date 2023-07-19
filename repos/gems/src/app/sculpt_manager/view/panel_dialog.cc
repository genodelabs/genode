/*
 * \brief  Panel dialog
 * \author Norman Feske
 * \date   2020-01-26
 */

/*
 * Copyright (C) 2020 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <view/panel_dialog.h>

using namespace Sculpt;


void Panel_dialog::view(Scope<> &s) const
{
	s.sub_scope<Frame>([&] (Scope<Frame> &s) {
		s.attribute("style", "unimportant");

		s.sub_scope<Float>([&] (Scope<Frame, Float> &s) {
			s.attribute("west", true);
			s.sub_scope<Hbox>([&] (Scope<Frame, Float, Hbox> &s) {

				if (_state.system_available())
					s.widget(_system_button, _state.system_visible());

				if (_state.settings_available())
					s.widget(_settings_button, _state.settings_visible());
			});
		});

		s.sub_scope<Float>([&] (Scope<Frame, Float> &s) {
			s.sub_scope<Hbox>([&] (Scope<Frame, Float, Hbox> &s) {

				Tab const tab = _state.selected_tab();

				s.widget(_files_tab,      tab);
				s.widget(_components_tab, tab);

				if (_state.inspect_tab_visible())
					s.widget(_inspect_tab, tab);
			});
		});

		s.sub_scope<Float>([&] (Scope<Frame, Float> &s) {
			s.attribute("east", true);
			s.sub_scope<Hbox>([&] (Scope<Frame, Float, Hbox> &s) {
				s.widget(_network_button, _state.network_visible());
				s.widget(_log_button,     _state.log_visible());
			});
		});
	});
}
