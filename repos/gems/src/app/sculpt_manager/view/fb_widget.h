/*
 * \brief  Framebuffer settings
 * \author Norman Feske
 * \date   2024-10-23
 */

/*
 * Copyright (C) 2024 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _VIEW__FB_WIDGET_H_
#define _VIEW__FB_WIDGET_H_

#include <view/dialog.h>
#include <model/fb_config.h>

namespace Sculpt { struct Fb_widget; }

struct Sculpt::Fb_widget : Widget<Vbox>
{
	using Connector     = Fb_connectors::Connector;
	using Mode          = Connector::Mode;
	using Hosted_choice = Hosted<Vbox, Choice<Mode::Id>>;
	using Mode_radio    = Hosted<Radio_select_button<Mode::Id>>;

	struct Action : Interface
	{
		virtual void select_fb_mode(Fb_connectors::Name const &, Mode::Id const &) = 0;
		virtual void disable_fb_connector(Fb_connectors::Name const &) = 0;
		virtual void toggle_fb_merge_discrete(Fb_connectors::Name const &) = 0;
		virtual void swap_fb_connector(Fb_connectors::Name const &) = 0;
		virtual void fb_brightness(Fb_connectors::Name const &, unsigned) = 0;
	};

	Fb_connectors::Name _selected_connector { };

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

					s.sub_scope<Float>([&] (auto &) { });
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

	using Hosted_brightness = Hosted<Bar>;

	void view(Scope<Vbox> &s, Fb_connectors const &connectors, Fb_config const &config,
	          Fb_connectors::Name const &hovered_display) const
	{
		auto view_connector = [&] (Connector const &conn)
		{
			Hosted_choice choice { Id { conn.name }, conn.name };

			Mode::Id selected_mode { "off" };
			conn._modes.for_each([&] (Mode const &mode) {
				if (mode.attr.used)
					selected_mode = mode.id; });

			s.widget(choice,
				Hosted_choice::Attr {
					.left_ex = 12, .right_ex = 28,
					.unfolded      = _selected_connector,
					.selected_item = selected_mode
				},
				[&] (Hosted_choice::Sub_scope &s) {

					if (conn.brightness.defined) {
						Hosted_brightness brightness { Id { "brightness" } };
						s.widget(brightness, conn.brightness.percent);
					}

					conn._modes.for_each([&] (Mode const &mode) {
						String<32> text { mode.attr.name };
						if (mode.attr.hz)
							text = { text, " (", mode.attr.hz, " Hz)" };

						s.widget(Mode_radio { Id { mode.id }, mode.id },
						         selected_mode, text);
					});
					if (conn.name != hovered_display)
						s.widget(Mode_radio { Id { "off" }, "off" }, selected_mode, "off");
				});
		};

		unsigned const num_merged = config.num_present_merged();

		auto view_controls = [&] (Scope<Vbox> &s, unsigned const count, Id const &id)
		{
			if (count <= 1)
				return;

			s.sub_scope<Float>([&] (Scope<Vbox, Float> &s) {
				s.sub_scope<Hbox>(id, [&] (Scope<Vbox, Float, Hbox> &s) {

					/*
					 * Restrict merge/unmerge toggle to last merged and first
					 * discrete connector.
					 */
					bool const toggle_allowed = (count == num_merged || count == num_merged + 1);
					Id const equal_id { toggle_allowed ? "equal" : "_equal" };

					s.sub_scope<Float>(equal_id,
						[&] (Scope<Vbox, Float, Hbox, Float> &s) {
						s.sub_scope<Button>([&] (Scope<Vbox, Float, Hbox, Float, Button> &s) {
							s.attribute("style", "vconn");
							if (count <= num_merged)
								s.attribute("selected", "yes");
							if (toggle_allowed) {
								if (s.hovered() && !s.dragged())
									s.attribute("hovered", "yes");
							}
							s.sub_node("hbox", [&] { });
						});
					});
					s.sub_scope<Float>(Id { "swap" }, [&] (Scope<Vbox, Float, Hbox, Float> &s) {
						s.sub_scope<Button>([&] (Scope<Vbox, Float, Hbox, Float, Button> &s) {
							s.attribute("style", "vswap");
							if (s.hovered() && !s.dragged())
								s.attribute("hovered", "yes");
							if (s.hovered() && s.dragged())
								s.attribute("selected", "yes");
							s.sub_node("hbox", [&] { });
						});
					});
				});
			});
		};

		unsigned count = 0;

		config.for_each_present_connector(connectors, [&] (Connector const &conn) {
			count++;
			view_controls(s, count, Id { conn.name });
			view_connector(conn);
		});
	}

	void click(Clicked_at const &at, Fb_connectors const &connectors, Action &action)
	{
		auto click_connector = [&] (Connector const &conn)
		{
			Hosted_choice choice { Id { conn.name }, conn.name };

			choice.propagate(at, _selected_connector,
				[&] { _selected_connector = { }; },
				[&] (Clicked_at const &at) {
					Id const id = at.matching_id<Mode_radio>();
					if (id.value == "brightness") {
						Hosted_brightness brightness { Id { "brightness" } };
						brightness.propagate(at, [&] (unsigned percent) {
							action.fb_brightness(conn.name, percent); });
					} else if (id.value == "off")
						action.disable_fb_connector(conn.name);
					else if (id.valid())
						action.select_fb_mode(conn.name, id);
				});
		};

		connectors._merged.for_each([&] (Connector const &conn) {
			click_connector(conn); });

		connectors._discrete.for_each([&] (Connector const &conn) {
			click_connector(conn); });

		/* operation buttons */
		{
			Id const conn = at.matching_id<Vbox, Float, Hbox>();
			Id const op   = at.matching_id<Vbox, Float, Hbox, Float>();

			if (op.value == "equal") action.toggle_fb_merge_discrete(conn.value);
			if (op.value == "swap")  action.swap_fb_connector(conn.value);
		}
	}
};

#endif /* _VIEW__FB_WIDGET_H_ */
