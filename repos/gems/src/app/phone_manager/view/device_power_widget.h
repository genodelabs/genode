/*
 * \brief  Device power-control widget
 * \author Norman Feske
 * \date   2022-11-18
 */

/*
 * Copyright (C) 2022 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _VIEW__DEVICE_POWER_WIDGET_H_
#define _VIEW__DEVICE_POWER_WIDGET_H_

#include <util/formatted_output.h>
#include <view/dialog.h>
#include <model/power_state.h>

namespace Sculpt { struct Device_power_widget; }


struct Sculpt::Device_power_widget : Widget<Vbox>
{
	enum class Option { UNKNOWN, PERFORMANCE, ECONOMIC, REBOOT, OFF };

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
			_performance { Id { "Performance" }, Option::PERFORMANCE },
			_economic    { Id { "Economic"    }, Option::ECONOMIC    },
			_reboot      { Id { "Reboot"      }, Option::REBOOT      },
			_off         { Id { "Power down"  }, Option::OFF         };

		void view(Scope<Float> &s, Option const &selected) const
		{
			s.sub_scope<Frame>([&] (Scope<Float, Frame> &s) {
				s.sub_scope<Vbox>([&] (Scope<Float, Frame, Vbox> &s) {
					s.widget(_performance, selected, Entry::Attr { .need_confirm = false });
					s.widget(_economic,    selected, Entry::Attr { .need_confirm = false });
					s.widget(_reboot,      selected, Entry::Attr { .need_confirm = true  });
					s.widget(_off,         selected, Entry::Attr { .need_confirm = true  });
					s.sub_scope<Min_ex>(35);
				});
			});
		}

		void click(Clicked_at const &at, Option const &selected, auto const &fn)
		{
			_performance.propagate(at, selected, fn);
			_economic   .propagate(at, selected, fn);
			_reboot     .propagate(at, selected, fn);
			_off        .propagate(at, selected, fn);
		}

		void clack(Clacked_at const &at, auto const &fn)
		{
			_reboot.propagate(at, fn);
			_off   .propagate(at, fn);
		}
	};

	static void _view_battery_value(Scope<> &s, auto const &label, double value, auto const &unit)
	{
		s.sub_scope<Hbox>([&] (Scope<Hbox> &s) {
			s.sub_scope<Label>(label, [&] (Scope<Hbox, Label> &s) {
				s.attribute("min_ex", 23); });

			auto pretty_value = [] (double value, auto unit)
			{
				using Text = String<64>;

				if (value < 1.0)
					return Text((unsigned)((value + 0.0005)*1000), " m", unit);

				unsigned const decimal   = (unsigned)(value + 0.005);
				unsigned const hundredth = (unsigned)(100*(value - decimal));

				return Text(decimal, ".",
				            Repeated(2 - printed_length(hundredth), "0"),
				            hundredth, " ", unit);
			};

			s.sub_scope<Label>(pretty_value(value, unit), [&] (Scope<Hbox, Label> &s) {
				s.attribute("min_ex", 8); });
		});
	}

	Hosted<Vbox, Power_options> _power_options { Id { "options" } };

	void view(Scope<Vbox> &s, Power_state const &power_state) const
	{
		auto curr_selection = [&]
		{
			if (_selected_option == Option::UNKNOWN) {

				if (power_state.profile == Power_state::Profile::PERFORMANCE)
					return Option::PERFORMANCE;

				if (power_state.profile == Power_state::Profile::ECONOMIC)
					return Option::ECONOMIC;
			}
			return _selected_option;
		};

		s.widget(_power_options, curr_selection());

		if (power_state.battery_present) {
			s.sub_scope<Centered_info_vbox>([&] (Scope<Vbox, Centered_info_vbox> &s) {

				s.as_new_scope([&] (Scope<> &s) {
					if (power_state.charging)
						_view_battery_value(s, "   Battery charge current ",
						                    power_state.battery.charge_current, "A");
					else
						_view_battery_value(s, "   Battery power draw ",
						                    power_state.battery.power_draw, "W");
				});

				s.sub_scope<Min_ex>(35);
			});
		}
	}

	struct Action : Interface
	{
		virtual void activate_performance_power_profile() = 0;
		virtual void activate_economic_power_profile() = 0;
		virtual void trigger_device_reboot() = 0;
		virtual void trigger_device_off() = 0;
	};

	void click(Clicked_at const &at, Action &action)
	{
		_power_options.propagate(at, _selected_option, [&] (Option const selected) {

			_selected_option = selected;

			if (selected == Option::PERFORMANCE) action.activate_performance_power_profile();
			if (selected == Option::ECONOMIC)    action.activate_economic_power_profile();
		});
	}

	void clack(Clacked_at const &at, Action &action)
	{
		_power_options.propagate(at, [&] (Option const confirmed) {

			if (confirmed == Option::REBOOT) action.trigger_device_reboot();
			if (confirmed == Option::OFF)    action.trigger_device_off();
		});
	}
};

#endif /* _VIEW__DEVICE_POWER_WIDGET_H_ */
