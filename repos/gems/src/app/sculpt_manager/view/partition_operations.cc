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

/* Genode includes */
#include <base/log.h>

/* local includes */
#include "partition_operations.h"

using namespace Sculpt;


void Partition_operations::view(Scope<Vbox> &s,
                                Storage_device const &device,
                                Partition      const &partition,
                                Storage_target const &used_target) const
{
	String<16> const version(device.label, ".", partition.number);

	bool const whole_device = !partition.number.valid();

	Storage_target const target { device.label, device.port, partition.number };

	bool const device_in_use = (used_target.device == device.label);

	bool const target_in_use = (used_target == target)
	                        || (whole_device && device_in_use)
	                        || partition.file_system.inspected;

	bool const relabel_in_progress = device.relabel_in_progress();
	bool const expand_in_progress  = device.expand_in_progress();

	if (partition.file_system.accessible()
	 && !_format.selected && !_expand.selected && !expand_in_progress) {

		if (!partition.check_in_progress
		 && !partition.format_in_progress
		 && !relabel_in_progress) {

			_fs_operations.view(s, target, used_target, partition.file_system);
		}

		if ((device.all_partitions_idle() || partition.relabel_in_progress())
		  && partition.genode() && !device_in_use) {

			s.widget(_relabel, [&] (Scope<Button> &s) {
				s.attribute("version", version);

				if (partition.genode_default() || partition.relabel_in_progress())
					s.attribute("selected", "yes");

				s.sub_scope<Label>("Default");
			});
			if (partition.relabel_in_progress())
				s.sub_scope<Label>("Relabeling in progress...");
		}

		if (!target_in_use && !partition.format_in_progress && partition.checkable()
		  && !relabel_in_progress) {

			s.widget(_check, [&] (Scope<Button> &s) {
				s.attribute("version", version);
				s.sub_scope<Label>("Check");

				if (partition.check_in_progress)
					s.xml.attribute("selected", "yes");
			});
			if (partition.check_in_progress)
				s.sub_scope<Label>("Check in progress...");
		}
	}

	bool const whole_device_with_partition_in_use =
		whole_device && !device.all_partitions_idle();

	bool const format_button_visible = !target_in_use
	                                && !whole_device_with_partition_in_use
	                                && !partition.check_in_progress
	                                && !expand_in_progress
	                                && !relabel_in_progress
	                                && !_expand.selected;

	bool const expand_button_visible = !target_in_use
	                                && !whole_device
	                                && !partition.check_in_progress
	                                && !partition.format_in_progress
	                                && !relabel_in_progress
	                                && partition.expandable()
	                                && !_format.selected;

	if (format_button_visible)
		_format.view(s, whole_device ? "Format device ..." : "Format partition ...");

	if (expand_button_visible)
		_expand.view(s, "Expand ...");

	if (partition.format_in_progress)
		s.sub_scope<Label>("Formatting in progress...");

	if (partition.gpt_expand_in_progress)
		s.sub_scope<Label>("Expanding partition...");

	if (partition.fs_resize_in_progress)
		s.sub_scope<Label>("Resizing file system...");
}


void Partition_operations::click(Clicked_at const &at, Storage_target const &partition,
                                 Storage_target const &used_target, Action &action)
{
	_format.click(at);
	_expand.click(at);

	_fs_operations.click(at, partition, used_target, action);

	_check  .propagate(at);
	_relabel.propagate(at);
}


void Partition_operations::clack(Clacked_at const &at, Storage_target const &partition,
                                 Action &action)
{
	_format.clack(at, [&] {
		if (_format.confirmed) {
			action.cancel_format(partition);
			_format.reset();
		} else {
			action.format(partition);
			_format.confirmed = true;
		}
	});

	_expand.clack(at, [&] {
		if (_expand.confirmed) {
			action.cancel_expand(partition);
			_expand.reset();
		} else {
			action.expand(partition);
			_expand.confirmed = true;
		}
	});

	_check.propagate(at, [&] {
		action.check(partition); });

	_relabel.propagate(at, [&] {
		action.toggle_default_storage_target(partition); });
}
