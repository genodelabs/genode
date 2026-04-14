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

#ifndef _VIEW__SYSTEM_OPTIONS_WIDGET_H_
#define _VIEW__SYSTEM_OPTIONS_WIDGET_H_

#include <model/options.h>
#include <view/dialog.h>
#include <string.h>

namespace Sculpt { struct System_options_widget; }


struct Sculpt::System_options_widget : Widget<Frame>
{
	Enabled_options const &_enabled_options;
	Options         const &_options;

	System_options_widget(Enabled_options const &enabled_options,
	                      Options         const &options)
	:
		_enabled_options(enabled_options), _options(options)
	{ }

	struct Option : Menu_entry
	{
		void view(Scope<Left_floating_hbox> &s, auto const &text, bool enabled) const
		{
			Menu_entry::view(s, enabled, Pretty(text), "checkbox");
		}
	};

	using Hosted_option = Hosted<Frame, Vbox, Option>;

	void view(Scope<Frame> &s) const
	{
		s.sub_scope<Vbox>([&] (Scope<Frame, Vbox> &s) {
			s.sub_scope<Min_ex>(35);
			unsigned count = 0;
			_options.for_each([&] (Options::Name const &name) {
				Hosted_option option { { count++ } };
				Enabled_options::Info const info = _enabled_options.info(name);
				if (!info.required)
					s.widget(option, name, info.exists);
			});
		});
	}

	struct Action : Interface
	{
		virtual void enable_option (Options::Name const &) = 0;
		virtual void disable_option(Options::Name const &) = 0;
	};

	void click(Clicked_at const &at, Action &action) const
	{
		Id const clicked_id = at.matching_id<Frame, Vbox, Option>();

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

#endif /* _VIEW__SYSTEM_OPTIONS_WIDGET_H_ */
