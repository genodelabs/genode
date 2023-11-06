/*
 * \brief  Test for Genode's dialog API
 * \author Norman Feske
 * \date   2023-03-25
 */

/*
 * Copyright (C) 2023 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <base/component.h>
#include <dialog/runtime.h>
#include <dialog/widgets.h>

namespace Dialog_test {
	using namespace Dialog;
	struct Main;
}


struct Dialog_test::Main
{
	Env &_env;
	Heap _heap { _env.ram(), _env.rm() };

	Runtime _runtime { _env, _heap };

	struct Main_dialog : Top_level_dialog
	{
		Hosted<Vbox, Action_button>          _inspect { Id { "Inspect" } };
		Hosted<Vbox, Deferred_action_button> _confirm { Id { "Confirm" } };
		Hosted<Vbox, Deferred_action_button> _cancel  { Id { "Cancel"  } };

		enum class Payment { CASH, CARD } _payment = Payment::CASH;

		using Payment_button = Select_button<Payment>;

		Hosted<Vbox, Hbox, Payment_button> _cash { Id { "Cash" }, Payment::CASH },
		                                   _card { Id { "Card" }, Payment::CARD };

		Main_dialog(Name const &name) : Top_level_dialog(name) { }

		struct Dishes : Widget<Vbox>
		{
			Id _items[4] { { "Pizza" }, { "Salad" }, { "Pasta" }, { "Soup" } };

			Id selected_item { };

			void view(Scope<Vbox> &s) const
			{
				for (Id const &id : _items) {
					s.sub_scope<Button>(id, [&] (Scope<Vbox, Button> &s) {

						bool const selected = (id == selected_item),
						           hovered  = (s.hovered() && (!s.dragged() || selected));

						if (selected) s.attribute("selected", "yes");
						if (hovered)  s.attribute("hovered",  "yes");

						s.sub_scope<Label>(id.value);
					});
				}
			}

			void click(Clicked_at const &at)
			{
				for (Id const &id : _items)
					if (at.matches<Vbox, Button>(id))
						selected_item = id;
			}
		};

		Hosted<Vbox, Frame, Dishes> _dishes { Id { "dishes" } };

		void view(Scope<> &s) const override
		{
			s.sub_scope<Vbox>([&] (Scope<Vbox> &s) {
				s.sub_scope<Min_ex>(15);

				s.sub_scope<Frame>([&] (Scope<Vbox, Frame> &s) {
					s.widget(_dishes); });

				if (_dishes.selected_item.valid()) {
					s.widget(_inspect);
					s.sub_scope<Hbox>([&] (Scope<Vbox, Hbox> &s) {
						s.widget(_cash, _payment);
						s.widget(_card, _payment);
					});
					s.widget(_confirm);
					s.widget(_cancel);
				}
			});
		}

		void click(Clicked_at const &at) override
		{
			_dishes .propagate(at);
			_inspect.propagate(at, [&] { log("inspect activated!"); });
			_confirm.propagate(at);
			_cancel .propagate(at);
			_cash   .propagate(at, [&] (Payment p) { _payment = p; });
			_card   .propagate(at, [&] (Payment p) { _payment = p; });
		}

		void clack(Clacked_at const &at) override
		{
			_confirm.propagate(at, [&] { log("confirm activated!"); });
			_cancel .propagate(at, [&] { _dishes.selected_item = { }; });
		}

	} _main_dialog { "main" };

	Runtime::View _main_view { _runtime, _main_dialog };

	/* handler used to respond to keyboard input */
	Runtime::Event_handler<Main> _event_handler { _runtime, *this, &Main::_handle_event };

	void _handle_event(Dialog::Event const &event)
	{
		log("_handle_event: ", event);
	}

	Main(Env &env) : _env(env) { }
};


void Component::construct(Genode::Env &env)
{
	static Dialog_test::Main main(env);
}

