/*
 * \brief  Widget for interacting with the current voice call
 * \author Norman Feske
 * \date   2022-06-30
 */

/*
 * Copyright (C) 2022-2023 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _VIEW__CURRENT_CALL_WIDGET_H_
#define _VIEW__CURRENT_CALL_WIDGET_H_

#include <view/dialog.h>
#include <model/current_call.h>

namespace Sculpt { struct Current_call_widget; }


struct Sculpt::Current_call_widget : Widget<Frame>
{
	struct Action : Interface
	{
		using Number = String<128>;

		virtual void accept_incoming_call() = 0;
		virtual void reject_incoming_call() = 0;
		virtual void hang_up() = 0;
		virtual void toggle_speaker() = 0;
		virtual void initiate_call() = 0;
		virtual void cancel_initiated_call() = 0;
		virtual void remove_last_dial_digit() = 0;
	};

	struct Active_call : Widget<Hbox>
	{
		using State = Current_call::State;

		Hosted<Hbox, Right_floating_hbox, Action_button>
			_accept  { Id { " Accept "  } },
			_reject  { Id { " Reject "  } },
			_cancel  { Id { " Cancel "  } },
			_hang_up { Id { " Hang up " } };

		Hosted<Hbox, Right_floating_hbox, Toggle_button>
			_speaker { Id { " Speaker " } };

		void view(Scope<Hbox> &s, Current_call const &call) const
		{
			auto message = [] (Current_call::State state)
			{
				switch (state) {
				case State::NONE:      break;
				case State::INCOMING:  return "Call from";
				case State::ACCEPTED:  return "Connected from";
				case State::REJECTED:  return "Disconnecting from";
				case State::HUNG_UP:   return "Disconnected from";
				case State::INITIATED: return "Dialing";
				case State::OUTBOUND:  return "Connecting to";
				case State::ALERTING:  return "Alerting";
				case State::ACTIVE:    return "Connected to";
				case State::CANCELED:  return "Canceled call to";
				}
				return "Failed"; /* never reached */
			};

			auto padded = [] (auto const &s) { return String<128>(" ", s, " "); };

			s.sub_scope<Vbox>([&] (Scope<Hbox, Vbox> &s) {
				s.sub_scope<Min_ex>(15);
				s.sub_scope<Label>(padded(message(call.state)));
				s.sub_scope<Label>(padded(call.number));
			});

			s.sub_scope<Right_floating_hbox>([&] (Scope<Hbox, Right_floating_hbox> &s) {

				if (call.incoming()) {
					s.widget(_accept);
					s.widget(_reject);
				}

				if (call.connecting())
					s.widget(_cancel);

				if (call.accepted() || call.active()) {
					s.widget(_speaker, call.speaker);
					s.widget(_hang_up);
				}
			});
		}

		void click(Clicked_at const &at, Action &action)
		{
			_reject .propagate(at, [&] { action.reject_incoming_call(); });
			_accept .propagate(at, [&] { action.accept_incoming_call(); });
			_speaker.propagate(at, [&] { action.toggle_speaker(); });
			_hang_up.propagate(at, [&] { action.hang_up(); });
			_cancel .propagate(at, [&] { action.cancel_initiated_call(); });
		}
	};

	struct Operations : Widget<Hbox>
	{
		struct Call_button : Action_button
		{
			void view(Scope<Button> &s, bool ready, auto text) const
			{
				Action_button::view(s, [&] (Scope<Button> &s) {
					if (!ready)
						s.attribute("style", "unimportant");
					s.sub_scope<Label>(text);
				});
			}
		};

		Hosted<Hbox, Float, Hbox, Call_button> _clear    { Id { "clear" } },
		                                       _initiate { Id { "initiate" } };;

		void view(Scope<Hbox> &s, Dialed_number const &number) const
		{
			s.sub_scope<Vbox>([&] (Scope<Hbox, Vbox> &s) {
				s.sub_scope<Vgap>();
				s.sub_scope<Vgap>(); });
			s.sub_scope<Float>([&] (Scope<Hbox, Float> &s) {
				s.sub_scope<Hbox>([&] (Scope<Hbox, Float, Hbox> &s) {
					s.widget(_clear, number.at_least_one_digit(), " Clear ");
					s.sub_scope<Min_ex>(2);
					s.widget(_initiate, number.suitable_for_call(), " Initiate ");
				});
			});
			s.sub_scope<Button_vgap>();
		}

		void click(Clicked_at const &at, Action &action)
		{
			_clear   .propagate(at, [&] { action.remove_last_dial_digit(); });
			_initiate.propagate(at, [&] { action.initiate_call(); });
		};
	};

	Hosted<Frame, Active_call> _active_call { Id { "active_call" } };
	Hosted<Frame, Operations>  _operations  { Id { "operations"  } };

	void view(Scope<Frame> &s, Dialed_number const &number, Current_call const &call) const
	{
		if (call.none()) {
			s.attribute("style", "invisible");
			s.widget(_operations, number);
		} else {
			s.attribute("style", "transient");
			s.widget(_active_call, call);
		}
	}

	void click(Clicked_at const &at, Action &action)
	{
		_active_call.propagate(at, action);
		_operations .propagate(at, action);
	}
};

#endif /* _VIEW__CURRENT_CALL_WIDGET_H_ */
