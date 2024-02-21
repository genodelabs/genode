/*
 * \brief  Widget for the software options
 * \author Norman Feske
 * \date   2022-09-20
 */

/*
 * Copyright (C) 2022 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _VIEW__SOFTWARE_OPTIONS_WIDGET_H_
#define _VIEW__SOFTWARE_OPTIONS_WIDGET_H_

#include <model/launchers.h>
#include <view/dialog.h>
#include <string.h>

namespace Sculpt { struct Software_options_widget; }


struct Sculpt::Software_options_widget : Widget<Vbox>
{
	Runtime_info const &_runtime_info;
	Launchers    const &_launchers;

	Software_options_widget(Runtime_info const &runtime_info,
	                        Launchers    const &launchers)
	:
		_runtime_info(runtime_info), _launchers(launchers)
	{ }

	struct Option : Widget<Frame>
	{
		Hosted<Frame, Right_floating_off_on> _switch { Id { "switch" } };

		void view(Scope<Frame> &s, auto const &text, bool enabled) const
		{
			s.attribute("style", "important");
			s.sub_scope<Left_floating_text>(Pretty(text));
			s.widget(_switch, enabled);
		}

		void click(Clicked_at const &at, auto const &fn) const
		{
			_switch.propagate(at, fn);
		}
	};

	using Hosted_option = Hosted<Vbox, Option>;

	void view(Scope<Vbox> &s) const
	{
		unsigned count = 0;
		_launchers.for_each([&] (Launchers::Info const &info) {
			Hosted_option option { { count++ } };
			s.widget(option,
			         info.path, _runtime_info.present_in_runtime(info.path));
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
			option.propagate(at, [&] (bool on) {
				if (on) action.enable_optional_component(info.path);
				else    action.disable_optional_component(info.path);
			});
		});
	}
};

#endif /* _VIEW__SOFTWARE_OPTIONS_WIDGET_H_ */
