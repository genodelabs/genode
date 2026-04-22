/*
 * \brief  Graph view of runtime state
 * \author Norman Feske
 * \date   2018-07-05
 */

/*
 * Copyright (C) 2018 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _GRAPH_H_
#define _GRAPH_H_

/* Genode includes */
#include <os/reporter.h>
#include <depot/archive.h>

/* local includes */
#include <types.h>
#include <view/storage_widget.h>
#include <view/ram_fs_widget.h>
#include <view/fb_widget.h>
#include <model/capacity.h>
#include <model/popup.h>
#include <model/runtime_config.h>
#include <model/runtime_state.h>
#include <model/component.h>
#include <string.h>

namespace Sculpt { struct Graph; }


struct Sculpt::Graph : Widget<Depgraph>
{
	struct Action : virtual Storage_device_widget::Action,
	                virtual Fb_widget::Action
	{
		virtual void grant_resource_request(Start_name const &) = 0;
		virtual void remove_deployed_component(Start_name const &) = 0;
		virtual void restart_deployed_component(Start_name const &) = 0;
		virtual void open_popup_dialog(Rect) = 0;
		virtual void view_child_dialog(Scope<> &) const = 0;
		virtual void click_child_dialog(Clicked_at const &) = 0;
		virtual void clack_child_dialog(Clacked_at const &) = 0;
	};

	Runtime_state  const &_runtime_state;
	Runtime_config       &_runtime_config;
	Storage_target const &_selected_target;
	Popup::State   const &_popup_state;

	Hosted<Depgraph, Toggle_button> _plus { Id { "+" } };

	Hosted<Depgraph, Frame, Vbox, Frame, Vbox, Hbox, Deferred_action_button>
		_remove  { Id { "Remove"  } },
		_restart { Id { "Restart" } };

	Hosted<Depgraph, Frame, Vbox, Frame, Vbox, Hbox, Action_button>
		_grant { Id { "Grant" } };

	struct Attr
	{
		Runtime_state::Ram  ram;
		Runtime_state::Caps caps;
		bool                alert;

		Runtime_config::Component::Pkg    pkg;
		Runtime_config::Component::Option option;
	};

	void _view_selected_node_content(Scope<Depgraph, Frame, Vbox> &,
	                                 Runtime_config::Component const &,
	                                 Action const &, Attr const &) const;

	Graph(Runtime_state  const &runtime_state, Runtime_config &runtime_config,
	      Storage_target const &selected_target, Popup::State const &popup_state)
	:
		_runtime_state(runtime_state), _runtime_config(runtime_config),
		_selected_target(selected_target), _popup_state(popup_state)
	{ }

	void view(Scope<Depgraph> &, Action const &) const;

	bool child_dialog_hovered(Hovered_at const &at) const
	{
		return at.matching_id<Depgraph, Frame, Vbox, Frame, Vbox>().valid();
	}

	void click(Clicked_at const &, Action &);
	void clack(Clacked_at const &, Action &, Ram_fs_widget::Action &);
};

#endif /* _GRAPH_H_ */
