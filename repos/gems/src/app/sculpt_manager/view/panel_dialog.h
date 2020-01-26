/*
 * \brief  Panel dialog
 * \author Norman Feske
 * \date   2020-01-26
 */

/*
 * Copyright (C) 2020 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _VIEW__PANEL_DIALOG_H_
#define _VIEW__PANEL_DIALOG_H_

/* Genode includes */
#include <os/reporter.h>

/* local includes */
#include <types.h>
#include <view/dialog.h>
#include <view/activatable_item.h>

namespace Sculpt { struct Panel_dialog; }


struct Sculpt::Panel_dialog : Dialog
{
	enum class Tab { FILES, COMPONENTS, INSPECT };

	struct State : Interface, Noncopyable
	{
		virtual Tab  selected_tab()        const = 0;
		virtual bool log_visible()         const = 0;
		virtual bool settings_visible()    const = 0;
		virtual bool network_visible()     const = 0;
		virtual bool inspect_tab_visible() const = 0;
	};

	State const &_state;

	struct Action : Interface
	{
		virtual void select_tab(Tab) = 0;
		virtual void toggle_log_visibility() = 0;
		virtual void toggle_settings_visibility() = 0;
		virtual void toggle_network_visibility() = 0;
	};

	Hoverable_item   _item        { };
	Activatable_item _action_item { };

	Hover_result hover(Xml_node hover) override
	{
		return any_hover_changed(
			_item.match(hover, "frame", "float", "hbox", "button", "name"));
	}

	void generate(Xml_generator &) const override;

	void click(Action &action)
	{
		if (_item.hovered("components")) action.select_tab(Tab::COMPONENTS);
		if (_item.hovered("files"))      action.select_tab(Tab::FILES);
		if (_item.hovered("inspect"))    action.select_tab(Tab::INSPECT);
		if (_item.hovered("log"))        action.toggle_log_visibility();
		if (_item.hovered("settings"))   action.toggle_settings_visibility();
		if (_item.hovered("network"))    action.toggle_network_visibility();
	}

	void reset() override
	{
		_item._hovered = Hoverable_item::Id();
		_action_item.reset();
	}

	Panel_dialog(State const &state) : _state(state) { }
};

#endif /* _VIEW__PANEL_DIALOG_H_ */
