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
#include <xml.h>
#include <view/activatable_item.h>
#include <view/storage_dialog.h>
#include <view/ram_fs_dialog.h>
#include <model/capacity.h>
#include <model/popup.h>
#include <model/runtime_config.h>
#include <model/runtime_state.h>
#include <model/component.h>
#include <string.h>

namespace Sculpt { struct Graph; }


struct Sculpt::Graph : Dialog
{
	Runtime_state        &_runtime_state;
	Runtime_config const &_runtime_config;

	Storage_devices const &_storage_devices;
	Storage_target  const &_sculpt_partition;
	Ram_fs_state    const &_ram_fs_state;

	Popup::State const &_popup_state;

	Depot_deploy::Children const &_deploy_children;

	Hoverable_item   _node_button_item { };
	Hoverable_item   _add_button_item { };
	Activatable_item _remove_item   { };

	/*
	 * Defined when '+' button is hovered
	 */
	Rect _popup_anchor { };

	Ram_fs_dialog _ram_fs_dialog;

	bool _storage_selected = false;
	bool _usb_selected     = false;

	bool _storage_dialog_visible() const { return _storage_selected || _usb_selected; }

	Reconstructible<Storage_dialog> _storage_dialog { _storage_devices, _sculpt_partition };

	void _gen_selected_node_content(Xml_generator &, Start_name const &,
	                                Runtime_state::Info const &) const;

	void _gen_parent_node(Xml_generator &, Start_name const &, Label const &) const;
	void _gen_storage_node(Xml_generator &) const;
	void _gen_usb_node(Xml_generator &) const;

	void generate(Xml_generator &) const override;

	Hover_result hover(Xml_node) override;

	void reset() override { }

	void reset_operation()
	{
		if (_storage_dialog.constructed())
			_storage_dialog->reset_operation();
	}

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
		_deploy_children(deploy_children), _ram_fs_dialog(sculpt_partition)
	{ }

	bool add_button_hovered() const { return _add_button_item._hovered.valid(); }

	struct Action : Storage_dialog::Action
	{
		virtual void remove_deployed_component(Start_name const &) = 0;
		virtual void toggle_launcher_selector(Rect) = 0;
	};

	void click(Action &action);
	void clack(Action &action, Ram_fs_dialog::Action &);
};

#endif /* _GRAPH_H_ */
