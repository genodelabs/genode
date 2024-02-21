/*
 * \brief  Device controls (e.g., brightness) widget
 * \author Norman Feske
 * \date   2022-11-22
 */

/*
 * Copyright (C) 2022 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _VIEW__DEVICE_CONTROLS_WIDGET_H_
#define _VIEW__DEVICE_CONTROLS_WIDGET_H_

#include <view/dialog.h>
#include <model/power_state.h>
#include <model/mic_state.h>
#include <model/audio_volume.h>

namespace Sculpt { struct Device_controls_widget; }


struct Sculpt::Device_controls_widget : Widget<Vbox>
{
	struct Level : Widget<Frame>
	{
		struct Bar : Widget<Right_floating_hbox>
		{
			void view(Scope<Right_floating_hbox> &s, unsigned const percent) const
			{
				for (unsigned i = 0; i < 10; i++) {
					s.sub_scope<Button>(Id { i }, [&] (Scope<Right_floating_hbox, Button> &s) {

						if (s.hovered()) s.attribute("hovered", "yes");

						if (i*10 <= percent)
							s.attribute("selected", "yes");
						else
							s.attribute("style", "unimportant");

						s.sub_scope<Label>(" ");
					});
				}
			}

			void click(Clicked_at const &at, auto const &fn)
			{
				Id const id = at.matching_id<Right_floating_hbox, Button>();
				unsigned value = 0;
				if (!ascii_to(id.value.string(), value))
					return;

				unsigned const percent = max(10u, min(100u, value*10 + 9));
				fn(percent);
			}
		};

		Hosted<Frame, Bar> _bar { Id { "bar" } };

		void view(Scope<Frame> &s, unsigned const percent) const
		{
			s.attribute("style", "important");

			s.sub_scope<Left_floating_text>(s.id.value);
			s.widget(_bar, percent);
		}

		void click(Clicked_at const &at, auto const &fn) { _bar.propagate(at, fn); }
	};

	Hosted<Vbox, Level> _brightness { Id { "Brightness" } },
	                    _volume     { Id { "Volume" } };

	struct Mic_choice : Widget<Frame>
	{
		using Mic_button = Hosted<Frame, Right_floating_hbox, Select_button<Mic_state> >;

		Mic_button _off   { Id { " Off " },   Mic_state::OFF   },
		           _phone { Id { " Phone " }, Mic_state::PHONE },
		           _on    { Id { " On " },    Mic_state::ON    };

		void view(Scope<Frame> &s, Mic_state state) const
		{
			s.attribute("style", "important");

			s.sub_scope<Left_floating_text>(s.id.value);
			s.sub_scope<Right_floating_hbox>([&] (Scope<Frame, Right_floating_hbox> &s) {
				s.widget(_off,   state);
				s.widget(_phone, state);
				s.widget(_on,    state);
			});
		}

		void click(Clicked_at const &at, auto const &fn)
		{
			_off  .propagate(at, [&] (Mic_state s) { fn(s); });
			_phone.propagate(at, [&] (Mic_state s) { fn(s); });
			_on   .propagate(at, [&] (Mic_state s) { fn(s); });
		}
	};

	Hosted<Vbox, Mic_choice> _mic_choice { Id { "Microphone" } };

	void view(Scope<Vbox> &s,
	          Power_state  const &power,
	          Mic_state    const &mic,
	          Audio_volume const &audio) const
	{
		s.widget(_brightness, power.brightness);
		s.sub_scope<Vgap>();
		s.widget(_volume, audio.value);
		s.sub_scope<Vgap>();
		s.widget(_mic_choice, mic);
	}

	struct Action : Interface
	{
		virtual void select_brightness_level(unsigned) = 0;
		virtual void select_volume_level(unsigned) = 0;
		virtual void select_mic_policy(Mic_state const &) = 0;
	};

	void _click_or_drag(Clicked_at const &at, Action &action)
	{
		_brightness.propagate(at, [&] (unsigned percent) {
			action.select_brightness_level(percent); });

		_volume.propagate(at, [&] (unsigned percent) {
			action.select_volume_level(percent); });
	}

	void click(Clicked_at const &at, Action &action)
	{
		_click_or_drag(at, action);

		_mic_choice.propagate(at, [&] (Mic_state policy) {
			action.select_mic_policy(policy); });
	}

	void drag(Dragged_at const &at, Action &action)
	{
		_click_or_drag(clicked_at(at), action);
	}
};

#endif /* _VIEW__DEVICE_CONTROLS_WIDGET_H_ */
