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
#include <util/xml_generator.h>
#include <types.h>

namespace Sculpt { struct File_operation_queue; }


struct Sculpt::File_operation_queue : Noncopyable
{
	/*
	 * Content for 'new_small_file', as used for creating depot-user meta data
	 */
	struct Content { String<256> string; };

	struct Operation : Interface
	{
		enum class State {
			PENDING,     /* scheduled for next fs_tool instance */
			IN_PROGRESS  /* processed by current fs_tool instance */
		};

		State state { State::PENDING };

		enum class Type { REMOVE_FILE, COPY_ALL_FILES, NEW_SMALL_FILE } type;

		Path const from { };
		Path const path; /* destination */

		Content const content { };

		Operation(Type type, Path const &path) : type(type), path(path) { }

		Operation(Type type, Path const &from, Path const &to)
		: type(type), from(from), path(to) { }

		Operation(Path const &path, Content const &content)
		: type(Type::NEW_SMALL_FILE), path(path), content(content) { }

		void gen_fs_tool_config(Xml_generator &xml) const
		{
			if (state != State::IN_PROGRESS)
				return;

			switch (type) {

			case Type::REMOVE_FILE:
				xml.node("remove-file", [&] {
					xml.attribute("path", path); });
				break;

			case Type::COPY_ALL_FILES:
				xml.node("copy-all-files", [&] {
					xml.attribute("from", from);
					xml.attribute("to",   path); });
				break;

			case Type::NEW_SMALL_FILE:
				xml.node("new-file", [&] {
					xml.attribute("path", path);
					xml.append_sanitized(content.string.string()); });
				break;
			}
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

		new (_alloc) Registered<Operation>(_operations,
		                                   Operation::Type::REMOVE_FILE, path);
	}

	void copy_all_files(Path const &from, Path const &to)
	{
		new (_alloc) Registered<Operation>(_operations,
		                                   Operation::Type::COPY_ALL_FILES, from, to);
	}

	bool copying_to_path(Path const &path) const
	{
		bool result = false;
		_operations.for_each([&] (Operation const &operation) {
			if (operation.path == path)
				if (operation.type == Operation::Type::COPY_ALL_FILES)
					result = true; });

		return result;
	}

	void new_small_file(Path const &path, Content const &content)
	{
		new (_alloc) Registered<Operation>(_operations, path, content);
	}

	bool any_operation_in_progress() const
	{
		bool any_in_progress = false;
		_operations.for_each([&] (Operation const &operation) {
			if (operation.state == Operation::State::IN_PROGRESS)
				any_in_progress = true; });

		return any_in_progress;
	}

	bool empty() const
	{
		bool result = true;
		_operations.for_each([&] (Operation const &) { result = false; });
		return result;
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
