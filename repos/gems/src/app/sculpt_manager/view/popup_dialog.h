/*
 * \brief  Popup dialog
 * \author Norman Feske
 * \date   2018-09-12
 */

/*
 * Copyright (C) 2018 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _VIEW__POPUP_DIALOG_H_
#define _VIEW__POPUP_DIALOG_H_

/* local includes */
#include <types.h>
#include <view/software_add_widget.h>

namespace Sculpt { struct Popup_dialog; }


struct Sculpt::Popup_dialog : Dialog::Top_level_dialog
{
	using Depot_users       = Rom_data;
	using Construction_info = Component::Construction_info;
	using Index             = Software_add_widget::Index;

	struct Action : virtual Software_add_widget::Action { };

	Action &_action;

	Hosted<Frame, Software_add_widget> _add;

	void view(Scope<> &s) const override
	{
		s.sub_scope<Frame>([&] (Scope<Frame> &s) {
			using Attr = Software_add_widget::Attr;
			s.widget(_add, Attr { .visible_frames     = false,
			                      .left_aligned_items = true });
		});
	}

	void click(Clicked_at const &at) override
	{
		_add.propagate(at, _action);
	}

	void clack(Clacked_at const &at) override
	{
		_add.propagate(at, _action);
	}

	void handle_key(Codepoint c, Action &action)
	{
		_add.handle_key(c, action);
	}

	Popup_dialog(Action &action,
	             Build_info         const &build_info,
	             Sculpt_version     const &sculpt_version,
	             Nic_state          const &nic_state,
	             Index_update_queue const &index_update_queue,
	             Index              const &index,
	             Download_queue     const &download_queue,
	             Runtime_config     const &runtime_config,
	             Dir_query          const &dir_query,
	             Depot_users        const &depot_users,
	             Construction_info  const &construction_info)
	:
		Top_level_dialog("popup"), _action(action),
		_add(Id { "add" }, build_info, sculpt_version, nic_state,
		     index_update_queue, index, download_queue, runtime_config,
		     dir_query, construction_info, depot_users)
	{ }

	bool watches_depot() const { return true; }

	bool keyboard_needed() const { return _add.keyboard_needed(); }

	void sanitize_user_selection() { _add.sanitize_user_selection(); }
};

#endif /* _VIEW__POPUP_DIALOG_H_ */
