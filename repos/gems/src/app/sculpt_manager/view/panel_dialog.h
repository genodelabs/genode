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

/* local includes */
#include <types.h>
#include <view/dialog.h>

namespace Sculpt { struct Panel_dialog; }


struct Sculpt::Panel_dialog : Top_level_dialog
{
	enum class Tab { FILES, COMPONENTS, INSPECT };

	struct State : Interface, Noncopyable
	{
		virtual Tab  selected_tab()        const = 0;
		virtual bool log_visible()         const = 0;
		virtual bool system_visible()      const = 0;
		virtual bool settings_visible()    const = 0;
		virtual bool network_visible()     const = 0;
		virtual bool inspect_tab_visible() const = 0;
		virtual bool system_available()    const = 0;
		virtual bool settings_available()  const = 0;

		bool inspect_tab_selected() const { return selected_tab() == Tab::INSPECT; }
		bool files_tab_selected()   const { return selected_tab() == Tab::FILES; }
	};

	State const &_state;

	struct Action : Interface
	{
		virtual void select_tab(Tab) = 0;
		virtual void toggle_log_visibility() = 0;
		virtual void toggle_system_visibility() = 0;
		virtual void toggle_settings_visibility() = 0;
		virtual void toggle_network_visibility() = 0;
	};

	Action &_action;

	Hosted<Frame, Float, Hbox, Toggle_button>
		_system_button   { Id { "System"   } },
		_settings_button { Id { "Settings" } },
		_network_button  { Id { "Network"  } },
		_log_button      { Id { "Log"      } };

	using Tab_button = Select_button<Tab>;

	Hosted<Frame, Float, Hbox, Tab_button>
		_files_tab      { Id { "Files"      }, Tab::FILES      },
		_components_tab { Id { "Components" }, Tab::COMPONENTS },
		_inspect_tab    { Id { "Inspect"    }, Tab::INSPECT    };

	void view(Scope<> &) const override;

	void click(Clicked_at const &at) override
	{
		_system_button  .propagate(at, [&] { _action.toggle_system_visibility(); });
		_settings_button.propagate(at, [&] { _action.toggle_settings_visibility(); });
		_network_button .propagate(at, [&] { _action.toggle_network_visibility(); });
		_log_button     .propagate(at, [&] { _action.toggle_log_visibility(); });
		_files_tab      .propagate(at, [&] (Tab t) { _action.select_tab(t); });
		_components_tab .propagate(at, [&] (Tab t) { _action.select_tab(t); });
		_inspect_tab    .propagate(at, [&] (Tab t) { _action.select_tab(t); });
	}

	Panel_dialog(State const &state, Action &action)
	: Top_level_dialog("panel"), _state(state), _action(action) { }
};

#endif /* _VIEW__PANEL_DIALOG_H_ */
