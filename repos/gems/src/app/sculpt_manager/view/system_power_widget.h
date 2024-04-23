/*
 * \brief  System power-control widget
 * \author Norman Feske
 * \date   2024-04-16
 */

/*
 * Copyright (C) 2024 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _VIEW__SYSTEM_POWER_WIDGET_H_
#define _VIEW__SYSTEM_POWER_WIDGET_H_

#include <util/formatted_output.h>
#include <view/dialog.h>

namespace Sculpt { struct System_power_widget; }


struct Sculpt::System_power_widget : Widget<Vbox>
{
	struct Supported
	{
		bool suspend, reset, poweroff;

		bool any_support() const { return suspend || reset || poweroff; }

		bool operator != (Supported const &other) const
		{
			return suspend  != other.suspend
			    || reset    != other.reset
			    || poweroff != other.poweroff;
		}
	};

	enum class Option { UNKNOWN, STANDBY, REBOOT, OFF };

	Option _selected_option { Option::UNKNOWN };

	struct Conditional_confirm : Widget<Right_floating_hbox>
	{
		Hosted<Right_floating_hbox, Deferred_action_button> _button { Id { } };

		void view(Scope<Right_floating_hbox> &s, bool condition) const
		{
			s.widget(_button, [&] (Scope<Button> &s) {

				if (!condition)
					s.attribute("style", "invisible");

				s.sub_scope<Label>("Confirm", [&] (auto &s) {
					if (!condition) s.attribute("style", "invisible"); });
			});
		}

		void click(Clicked_at const &at) { _button.propagate(at); }

		void clack(Clacked_at const &at, auto const &confirmed_fn)
		{
			_button.propagate(at, confirmed_fn);
		}
	};

	struct Power_options : Widget<Float>
	{
		struct Entry : Widget<Hbox>
		{
			struct Attr { bool need_confirm; };

			Hosted<Hbox, Radio_select_button<Option>> _radio;
			Hosted<Hbox, Conditional_confirm>         _confirm { Id { "confirm" } };

			Entry(Option const option) : _radio(Id { "radio" }, option) { }

			void view(Scope<Hbox> &s, Option const &selected, Attr attr) const
			{
				s.widget(_radio, selected, s.id.value);
				s.widget(_confirm, attr.need_confirm && (selected == _radio.value));
			}

			void click(Clicked_at const &at, Option const &selected, auto const &fn)
			{
				_radio.propagate(at, fn);

				if (selected == _radio.value)
					_confirm.propagate(at);
			}

			void clack(Clacked_at const &at, auto const &fn)
			{
				_confirm.propagate(at, [&] { fn(_radio.value); });
			}
		};

		Hosted<Float, Frame, Vbox, Entry>
			_suspend     { Id { "Standby"         }, Option::STANDBY },
			_reboot      { Id { "Hard reboot"     }, Option::REBOOT  },
			_off         { Id { "Hard power down" }, Option::OFF     };

		void view(Scope<Float> &s, Option const &selected, Supported const supported) const
		{
			s.sub_scope<Frame>([&] (Scope<Float, Frame> &s) {
				s.sub_scope<Vbox>([&] (Scope<Float, Frame, Vbox> &s) {
					Entry::Attr const attr { .need_confirm = true };
					if (supported.suspend)  s.widget(_suspend, selected, attr);
					if (supported.reset)    s.widget(_reboot,  selected, attr);
					if (supported.poweroff) s.widget(_off,     selected, attr);
					s.sub_scope<Min_ex>(35);
				});
			});
		}

		void click(Clicked_at const &at, Option const &selected, auto const &fn)
		{
			_suspend.propagate(at, selected, fn);
			_reboot .propagate(at, selected, fn);
			_off    .propagate(at, selected, fn);
		}

		void clack(Clacked_at const &at, auto const &fn)
		{
			_suspend.propagate(at, fn);
			_reboot .propagate(at, fn);
			_off    .propagate(at, fn);
		}
	};

	Hosted<Vbox, Power_options> _power_options { Id { "options" } };

	void view(Scope<Vbox> &s, Supported const supported) const
	{
		s.widget(_power_options, _selected_option, supported);
	}

	struct Action : Interface
	{
		virtual void trigger_suspend() = 0;
		virtual void trigger_reboot() = 0;
		virtual void trigger_power_off() = 0;
	};

	void click(Clicked_at const &at)
	{
		_power_options.propagate(at, _selected_option, [&] (Option const selected) {
			_selected_option = selected; });
	}

	void clack(Clacked_at const &at, Action &action)
	{
		_power_options.propagate(at, [&] (Option const confirmed) {

			if (confirmed == Option::STANDBY) action.trigger_suspend();
			if (confirmed == Option::REBOOT)  action.trigger_reboot();
			if (confirmed == Option::OFF)     action.trigger_power_off();
		});
	}
};

#endif /* _VIEW__SYSTEM_POWER_WIDGET_H_ */
