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

/* included from depot_deploy tool */
#include <children.h>

/* local includes */
#include <types.h>
#include <view/storage_widget.h>
#include <view/ram_fs_widget.h>
#include <model/capacity.h>
#include <model/popup.h>
#include <model/runtime_config.h>
#include <model/runtime_state.h>
#include <model/component.h>
#include <string.h>

namespace Sculpt { struct Graph; }


struct Sculpt::Graph : Widget<Depgraph>
{
	Runtime_state                &_runtime_state;
	Runtime_config         const &_runtime_config;
	Storage_devices        const &_storage_devices;
	Storage_target         const &_sculpt_partition;
	Ram_fs_state           const &_ram_fs_state;
	Popup::State           const &_popup_state;
	Depot_deploy::Children const &_deploy_children;

	Hosted<Depgraph, Toggle_button> _plus { Id { "+" } };

	Hosted<Depgraph, Frame, Vbox, Ram_fs_widget>
		_ram_fs_widget { Id { "ram_fs" } };

	Hosted<Depgraph, Frame, Vbox, Frame, Hbox, Deferred_action_button>
		_remove  { Id { "Remove"  } },
		_restart { Id { "Restart" } };

	Hosted<Depgraph, Frame, Vbox, Frame, Block_devices_widget>
		_block_devices_widget { Id { "block_devices" },
		                        _storage_devices, _sculpt_partition };

	Hosted<Depgraph, Frame, Vbox, Frame, Usb_devices_widget>
		_usb_devices_widget { Id { "usb_devices" },
		                        _storage_devices, _sculpt_partition };

	bool _storage_selected = false;

	void _view_selected_node_content(Scope<Depgraph, Frame, Vbox> &,
	                                 Start_name const &,
	                                 Runtime_state::Info const &) const;

	void _view_storage_node(Scope<Depgraph> &) const;

	Graph(Runtime_state                &runtime_state,
	      Runtime_config         const &runtime_config,
	      Storage_devices        const &storage_devices,
	      Storage_target         const &sculpt_partition,
	      Ram_fs_state           const &ram_fs_state,
	      Popup::State           const &popup_state,
	      Depot_deploy::Children const &deploy_children)
	:
		_runtime_state(runtime_state), _runtime_config(runtime_config),
		_storage_devices(storage_devices), _sculpt_partition(sculpt_partition),
		_ram_fs_state(ram_fs_state), _popup_state(popup_state),
		_deploy_children(deploy_children)
	{ }

	void view(Scope<Depgraph> &) const;

	struct Action : Storage_device_widget::Action
	{
		virtual void remove_deployed_component(Start_name const &) = 0;
		virtual void restart_deployed_component(Start_name const &) = 0;
		virtual void open_popup_dialog(Rect) = 0;
	};

	void click(Clicked_at const &, Action &);
	void clack(Clacked_at const &, Action &, Ram_fs_widget::Action &);

	void reset_storage_operation()
	{
		_block_devices_widget.reset_operation();
		_usb_devices_widget.reset_operation();
	}
};

#endif /* _GRAPH_H_ */
