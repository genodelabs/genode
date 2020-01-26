/*
 * \brief  Menu-view dialog handling
 * \author Norman Feske
 * \date   2018-05-18
 */

/*
 * Copyright (C) 2018 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _MENU_VIEW_H_
#define _MENU_VIEW_H_

/* Genode includes */
#include <base/session_label.h>
#include <os/reporter.h>
#include <base/attached_rom_dataspace.h>

/* local includes */
#include "types.h"
#include <view/dialog.h>
#include <model/child_state.h>

namespace Sculpt { struct Menu_view; }


struct Sculpt::Menu_view : Noncopyable
{
	Dialog &_dialog;

	Child_state _child_state;

	Expanding_reporter _dialog_reporter;

	Attached_rom_dataspace _hover_rom;

	Signal_handler<Menu_view> _hover_handler;

	bool _hovered = false;

	unsigned min_width  = 0;
	unsigned min_height = 0;

	void _handle_hover();

	void _gen_start_node_content(Xml_generator &) const;

	Menu_view(Env &, Registry<Child_state> &registry,
	          Dialog &, Start_name const &, Ram_quota, Cap_quota,
	          Session_label const &dialog_report_name,
	          Session_label const &hover_rom_name);

	void generate();

	bool hovered() const { return _hovered; }

	void reset();

	void gen_start_node(Xml_generator &) const;

	bool apply_child_state_report(Xml_node report)
	{
		return _child_state.apply_child_state_report(report);
	}

	void trigger_restart() { _child_state.trigger_restart(); }
};

#endif /* _MENU_VIEW_H_ */
