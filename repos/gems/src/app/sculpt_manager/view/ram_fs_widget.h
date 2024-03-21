/*
 * \brief  RAM file-system management widget
 * \author Norman Feske
 * \date   2020-01-28
 */

/*
 * Copyright (C) 2020 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _VIEW__RAM_FS_WIDGET_H_
#define _VIEW__RAM_FS_WIDGET_H_

#include <view/fs_operations.h>
#include <model/ram_fs_state.h>

namespace Sculpt { struct Ram_fs_widget; }


struct Sculpt::Ram_fs_widget : Widget<Vbox>
{
	Storage_target const _target { "ram_fs", { }, Partition::Number() };

	struct Action : Interface, Noncopyable
	{
		virtual void reset_ram_fs() = 0;
	};

	Fs_operations               _fs    { };
	Doublechecked_action_button _reset { "reset" };

	void view(Scope<Vbox> &s,
	          Storage_target const &used_target,
	          Ram_fs_state   const &ram_fs_state) const
	{
		_fs.view(s, _target, used_target, ram_fs_state);

		if (!used_target.ram_fs() && !ram_fs_state.inspected)
			_reset.view(s, "Reset ...");
	}

	void click(Clicked_at     const &at,
	           Storage_target const &used_target,
	           Fs_operations::Action &action)
	{
		_reset.click(at);
		_fs.click(at, _target, used_target, action);
	}

	void clack(Clacked_at const &at, Action &action)
	{
		_reset.clack(at, [&] {
			action.reset_ram_fs();
			_reset.selected = false;
		});
	}
};

#endif /* _VIEW__RAM_FS_WIDGET_H_ */
