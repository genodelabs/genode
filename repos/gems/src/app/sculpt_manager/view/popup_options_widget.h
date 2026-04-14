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

#include <model/options.h>
#include <view/dialog.h>
#include <string.h>

namespace Sculpt { struct Popup_options_widget; }


struct Sculpt::Popup_options_widget : Widget<Vbox>
{
	Runtime_info    const &_runtime_info;
	Enabled_options const &_enabled_options;
	Options         const &_options;

	Popup_options_widget(Runtime_info    const &runtime_info,
	                     Enabled_options const &enabled_options,
	                     Options         const &options)
	:
		_runtime_info(runtime_info), _enabled_options(enabled_options),
		_options(options)
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
		_options.for_each([&] (Options::Name const &name) {
			Hosted_option option { { count++ } };
			Enabled_options::Info const info = _enabled_options.info(name);
			if (!info.required)
				s.widget(option, name, info.exists);
		});
	}

	struct Action : Interface
	{
		virtual void enable_option (Options::Name const &) = 0;
		virtual void disable_option(Options::Name const &) = 0;
	};

	void click(Clicked_at const &at, Action &action) const
	{
		Id const clicked_id = at.matching_id<Vbox, Option>();

		unsigned count = 0;

		_options.for_each([&] (Options::Name const &name) {
			Id const id { { count++ } };
			if (clicked_id != id)
				return;

			Hosted_option const option { id };
			option.propagate(at, [&] {
				if (_enabled_options.info(name).exists)
					action.disable_option(name);
				else
					action.enable_option(name);
			});
		});
	}
};

#endif /* _VIEW__POPUP_OPTIONS_WIDGET_H_ */
