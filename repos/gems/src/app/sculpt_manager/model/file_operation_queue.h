/*
 * \brief  List of file operations that are currently in flight
 * \author Norman Feske
 * \date   2019-03-12
 */

/*
 * Copyright (C) 2019 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _MODEL__FILE_OPERATION_QUEUE_H_
#define _MODEL__FILE_OPERATION_QUEUE_H_

#include <base/registry.h>
#include <types.h>

namespace Sculpt { struct File_operation_queue; }


struct Sculpt::File_operation_queue : Noncopyable
{
	struct Operation : Interface
	{
		enum class State {
			PENDING,     /* scheduled for next fs_tool instance */
			IN_PROGRESS  /* processed by current fs_tool instance */
		};

		State state { State::PENDING };

		enum class Type { REMOVE_FILE } type;

		Path const path;

		Operation(Type type, Path const &path) : type(type), path(path) { }

		void gen_fs_tool_config(Xml_generator &xml) const
		{
			if (state != State::IN_PROGRESS)
				return;

			xml.node("remove-file", [&] () {
				xml.attribute("path", path); });
		}
	};

	Allocator &_alloc;

	Registry<Registered<Operation> > _operations { };

	File_operation_queue(Allocator &alloc) : _alloc(alloc) { }

	void remove_file(Path const &path)
	{
		bool already_exists = false;
		_operations.for_each([&] (Operation const &operation) {
			if (operation.type == Operation::Type::REMOVE_FILE && operation.path == path)
				already_exists = true; });

		if (already_exists)
			return;

		new (_alloc) Registered<Operation>(_operations, Operation::Type::REMOVE_FILE, path);
	}

	bool any_operation_in_progress() const
	{
		bool any_in_progress = false;
		_operations.for_each([&] (Operation const &operation) {
			if (operation.state == Operation::State::IN_PROGRESS)
				any_in_progress = true; });

		return any_in_progress;
	}

	void schedule_next_operations()
	{
		/*
		 * All operations that were in progress are complete. So they can
		 * be removed from the queue. All pending operations become the
		 * operations-in-progress in the next iteration.
		 */
		_operations.for_each([&] (Registered<Operation> &operation) {
			switch (operation.state) {

			case Operation::State::IN_PROGRESS:
				destroy(_alloc, &operation);
				break;

			case Operation::State::PENDING:
				operation.state = Operation::State::IN_PROGRESS;
				break;
			}
		});
	}

	void gen_fs_tool_config(Xml_generator &xml) const
	{
		_operations.for_each([&] (Operation const &operation) {
			operation.gen_fs_tool_config(xml); });
	}
};

#endif /* _MODEL__FILE_OPERATION_QUEUE_H_ */
