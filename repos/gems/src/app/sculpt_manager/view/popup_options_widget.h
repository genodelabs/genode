/*
 * \brief  Widget for the software options
 * \author Norman Feske
 * \date   2024-04-02
 */

/*
 * Copyright (C) 2024 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _VIEW__POPUP_OPTIONS_WIDGET_H_
#define _VIEW__POPUP_OPTIONS_WIDGET_H_

#include <model/launchers.h>
#include <view/dialog.h>
#include <string.h>

namespace Sculpt { struct Popup_options_widget; }


struct Sculpt::Popup_options_widget : Widget<Vbox>
{
	Runtime_info const &_runtime_info;
	Launchers    const &_launchers;

	Popup_options_widget(Runtime_info const &runtime_info,
	                     Launchers    const &launchers)
	:
		_runtime_info(runtime_info), _launchers(launchers)
	{ }

	struct Option : Menu_entry
	{
		void view(Scope<Left_floating_hbox> &s, auto const &text, bool enabled) const
		{
			Menu_entry::view(s, enabled, Pretty(text), "checkbox");
		}
	};

	using Hosted_option = Hosted<Vbox, Option>;

	void view(Scope<Vbox> &s) const
	{
		unsigned count = 0;
		_launchers.for_each([&] (Launchers::Info const &info) {
			Hosted_option option { { count++ } };
			s.widget(option, info.path, _runtime_info.present_in_runtime(info.path));
		});
	}

	struct Action : Interface
	{
		virtual void enable_optional_component (Path const &launcher) = 0;
		virtual void disable_optional_component(Path const &launcher) = 0;
	};

	void click(Clicked_at const &at, Action &action) const
	{
		Id const clicked_id = at.matching_id<Vbox, Option>();

		unsigned count = 0;
		_launchers.for_each([&] (Launchers::Info const &info) {
			Id const id { { count++ } };
			if (clicked_id != id)
				return;

			Hosted_option const option { id };
			option.propagate(at, [&] {
				if (_runtime_info.present_in_runtime(info.path))
					action.disable_optional_component(info.path);
				else
					action.enable_optional_component(info.path);
			});
		});
	}
};

#endif /* _VIEW__POPUP_OPTIONS_WIDGET_H_ */
