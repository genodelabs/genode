/*
 * \brief  Resource assignment widget
 * \author Norman Feske
 * \date   2023-11-01
 */

/*
 * Copyright (C) 2023 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _VIEW__RESOURCE_WIDGET_H_
#define _VIEW__RESOURCE_WIDGET_H_

/* local includes */
#include <model/component.h>
#include <view/dialog.h>

namespace Sculpt { struct Resource_widget; }


struct Sculpt::Resource_widget : Widget<Vbox>
{
	template <typename WIDGET>
	struct Titled_widget : Widget<Left_floating_hbox>
	{
		Hosted<Left_floating_hbox, Vbox, WIDGET> _hosted;

		Titled_widget(auto &&... args) : _hosted(Id { "hosted" }, args...) { }

		void view(Scope<Left_floating_hbox> &s, auto const &text, auto &&... args) const
		{
			/* title */
			s.sub_scope<Vbox>([&] (Scope<Left_floating_hbox, Vbox> &s) {
				s.sub_scope<Top_left_floating_hbox>([&] (auto &s) {

					/*
					 * The button is used to vertically align the "Priority"
					 * label with the text of the first radio button.
					 * The leading space is used to horizontally align
					 * the label with the "Resource assignment ..." dialog
					 * title.
					 */
					s.template sub_scope<Button>([&] (auto &s) {
						s.attribute("style", "invisible");
						s.sub_node("hbox", [&] { }); });

					s.template sub_scope<Label>(String<32>(" ", text));
				});
				s.sub_scope<Min_ex>(11);
			});

			s.sub_scope<Vbox>([&] (Scope<Left_floating_hbox, Vbox> &s) {
				s.widget(_hosted, args...); });

			s.sub_scope<Hbox>();
		}

		void click(auto &&... args) { _hosted.propagate(args...); }
	};

	struct Priority_selector : Widget<Vbox>
	{
		Hosted<Vbox, Radio_select_button<Priority>>
			_buttons[4] { { Id { "Driver"     }, Priority::DRIVER     },
			              { Id { "Multimedia" }, Priority::MULTIMEDIA },
			              { Id { "Default"    }, Priority::DEFAULT    },
			              { Id { "Background" }, Priority::BACKGROUND } };

		void view(Scope<Vbox> &s, Priority const &priority) const
		{
			for (auto const &button : _buttons)
				s.widget(button, priority);
		}

		void click(Clicked_at const &at, Priority &priority)
		{
			for (auto &button : _buttons)
				button.propagate(at, [&] (Priority value) { priority = value; });
		}
	};

	struct Affinity_selector : Widget<Vbox>
	{
		void view(Scope<Vbox> &, Affinity::Space const &, Affinity::Location const &) const;

		void click(Clicked_at const &, Affinity::Space const &, Affinity::Location &);
	};

	Hosted<Vbox, Titled_widget<Affinity_selector>> _affinity { Id { "affinity" } };
	Hosted<Vbox, Titled_widget<Priority_selector>> _priority { Id { "priority" } };
	Hosted<Vbox, Menu_entry>                       _system   { Id { "system control" } };

	void view(Scope<Vbox> &s, Component const &component) const
	{
		s.sub_scope<Small_vgap>();

		if (component.affinity_space.total() > 1) {
			s.widget(_affinity, "Affinity", component.affinity_space, component.affinity_location);
			s.sub_scope<Small_vgap>();
		}

		s.widget(_priority, "Priority", component.priority);
		s.sub_scope<Small_vgap>();

		s.widget(_system, component.system_control, "System control", "checkbox");
	}

	void click(Clicked_at const &at, Component &component)
	{
		if (component.affinity_space.total() > 1)
			_affinity.propagate(at, component.affinity_space, component.affinity_location);

		_priority.propagate(at, component.priority),

		_system.propagate(at, [&] {
			component.system_control = !component.system_control; });
	}
};

#endif /* _VIEW__RESOURCE_WIDGET_H_ */
