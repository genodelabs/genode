/*
 * \brief  Common part of file-system management dialogs
 * \author Norman Feske
 * \date   2020-01-28
 */

/*
 * Copyright (C) 2020 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _VIEW__FS_OPERATIONS_H_
#define _VIEW__FS_OPERATIONS_H_

#include <feature.h>
#include <view/dialog.h>
#include <model/storage_target.h>

namespace Sculpt { struct Fs_operations; }

struct Sculpt::Fs_operations
{
	struct Action : Interface
	{
		virtual void toggle_inspect_view(Storage_target const &) = 0;

		virtual void use(Storage_target const &) = 0;
	};

	Hosted<Vbox, Toggle_button> _inspect { Id { "Inspect" } };
	Hosted<Vbox, Toggle_button> _use     { Id { "Use" } };

	void view(Scope<Vbox> &s,
	          Storage_target const &target,
	          Storage_target const &used_target,
	          File_system    const &file_system) const
	{
		if (Feature::INSPECT_VIEW)
			s.widget(_inspect, file_system.inspected);

		bool const selected_for_use = (used_target == target);

		if (!used_target.valid() || selected_for_use)
			s.widget(_use, selected_for_use);
	}

	void click(Clicked_at const &at,
	           Storage_target const &target,
	           Storage_target const &used_target, Action &action)
	{
		_inspect.propagate(at, [&] {
			action.toggle_inspect_view(target); });

		_use.propagate(at, [&] {
			action.use((used_target == target) ? Storage_target{ } : target); });
	}
};

#endif /* _VIEW__FS_OPERATIONS_H_ */
