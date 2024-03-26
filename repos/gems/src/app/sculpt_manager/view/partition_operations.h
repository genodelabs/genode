/*
 * \brief  Partition management dialog
 * \author Norman Feske
 * \date   2020-01-29
 */

/*
 * Copyright (C) 2020 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _VIEW__PARTITION_OPERATIONS_H_
#define _VIEW__PARTITION_OPERATIONS_H_

#include <types.h>
#include <model/storage_devices.h>
#include <view/fs_operations.h>

namespace Sculpt { struct Partition_operations; }


struct Sculpt::Partition_operations
{
	Hosted<Vbox, Deferred_action_button>
		_relabel { Id { "default" } },
		_check   { Id { "check" } };

	Doublechecked_action_button _format { "format" }, _expand { "expand" };

	Fs_operations _fs_operations { };

	void view(Scope<Vbox> &s, Storage_device const &, Partition const &,
	          Storage_target const &used_target) const;

	struct Action : virtual Fs_operations::Action
	{
		virtual void format(Storage_target const &) = 0;
		virtual void cancel_format(Storage_target const &) = 0;
		virtual void expand(Storage_target const &) = 0;
		virtual void cancel_expand(Storage_target const &) = 0;
		virtual void check(Storage_target const &) = 0;
		virtual void toggle_default_storage_target(Storage_target const &) = 0;
	};

	void reset_operation()
	{
		_format.reset();
		_expand.reset();
	}

	void click(Clicked_at const &, Storage_target const &partition, Storage_target const &, Action &);
	void clack(Clacked_at const &, Storage_target const &partition, Action &);
};

#endif /* _VIEW__PARTITION_OPERATIONS_H_ */
