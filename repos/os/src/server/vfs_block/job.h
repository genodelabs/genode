/*
 * \brief  VFS file to Block session
 * \author Josef Soentgen
 * \date   2020-05-05
 */

/*
 * Copyright (C) 2020 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _VFS_BLOCK__JOB_
#define _VFS_BLOCK__JOB_

namespace Vfs_block {

	using file_size = Vfs::file_size;
	using file_offset = Vfs::file_offset;

	struct Job
	{
		struct Unsupported_Operation : Genode::Exception { };
		struct Invalid_state         : Genode::Exception { };

		enum State { PENDING, IN_PROGRESS, COMPLETE, };

		static State _initial_state(Block::Operation::Type type)
		{
			using Type = Block::Operation::Type;

			switch (type) {
			case Type::READ:  return State::PENDING;
			case Type::WRITE: return State::PENDING;
			case Type::TRIM:  return State::PENDING;
			case Type::SYNC:  return State::PENDING;
			default:          throw Unsupported_Operation();
			}
		}

		static char const *_state_to_string(State s)
		{
			switch (s) {
			case State::PENDING:      return "PENDING";
			case State::IN_PROGRESS:  return "IN_PROGRESS";
			case State::COMPLETE:     return "COMPLETE";
			}

			throw Invalid_state();
		}

		Vfs::Vfs_handle &_handle;

		Block::Request const request;
		char              *data;
		State              state;
		file_offset const  base_offset;
		file_offset        current_offset;
		file_size          current_count;

		bool success;
		bool complete;

		bool _read()
		{
			bool progress = false;

			switch (state) {
			case State::PENDING:

				_handle.seek(base_offset + current_offset);
				if (!_handle.fs().queue_read(&_handle, current_count)) {
					return progress;
				}

				state = State::IN_PROGRESS;
				progress = true;
			[[fallthrough]];
			case State::IN_PROGRESS:
			{
				using Result = Vfs::File_io_service::Read_result;

				bool completed = false;
				file_size out = 0;

				Result const result =
					_handle.fs().complete_read(&_handle,
					                           data + current_offset,
					                           current_count, out);
				if (result == Result::READ_QUEUED
				 || result == Result::READ_ERR_INTERRUPT
				 || result == Result::READ_ERR_WOULD_BLOCK) {
					return progress;
				} else

				if (result == Result::READ_OK) {
					current_offset += out;
					current_count  -= out;
					success = true;
				} else

				if (   result == Result::READ_ERR_IO
				    || result == Result::READ_ERR_INVALID) {
					success   = false;
					completed = true;
				}

				if (current_count == 0 || completed) {
					state = State::COMPLETE;
				} else {
					state = State::PENDING;
					/* partial read, keep trying */
					return true;
				}
				progress = true;
			}
			[[fallthrough]];
			case State::COMPLETE:

				complete = true;
				progress = true;
			default: break;
			}

			return progress;
		}

		bool _write()
		{
			bool progress = false;

			switch (state) {
			case State::PENDING:

				_handle.seek(base_offset + current_offset);
				state = State::IN_PROGRESS;
				progress = true;

			[[fallthrough]];
			case State::IN_PROGRESS:
			{
				using Result = Vfs::File_io_service::Write_result;

				bool completed = false;
				file_size out = 0;

				Result result = _handle.fs().write(&_handle,
				                                   data + current_offset,
				                                   current_count, out);
				switch (result) {
				case Result::WRITE_ERR_WOULD_BLOCK:
					return progress;

				case Result::WRITE_OK:
					current_offset += out;
					current_count  -= out;
					success = true;
					break;

				case Result::WRITE_ERR_IO:
				case Result::WRITE_ERR_INVALID:
					success = false;
					completed = true;
					break;
				}

				if (current_count == 0 || completed) {
					state = State::COMPLETE;
				} else {
					state = State::PENDING;
					/* partial write, keep trying */
					return true;
				}
				progress = true;
			}
			[[fallthrough]];
			case State::COMPLETE:

				complete = true;
				progress = true;
			default: break;
			}

			return progress;
		}

		bool _sync()
		{
			bool progress = false;

			switch (state) {
			case State::PENDING:

				if (!_handle.fs().queue_sync(&_handle)) {
					return progress;
				}
				state = State::IN_PROGRESS;
				progress = true;
			[[fallthrough]];
			case State::IN_PROGRESS:
			{
				using Result = Vfs::File_io_service::Sync_result;
				Result const result = _handle.fs().complete_sync(&_handle);

				if (result == Result::SYNC_QUEUED) {
					return progress;
				} else

				if (result == Result::SYNC_ERR_INVALID) {
					success = false;
				} else

				if (result == Result::SYNC_OK) {
					success = true;
				}

				state = State::COMPLETE;
				progress = true;
			}
			[[fallthrough]];
			case State::COMPLETE:

				complete = true;
				progress = true;
			default: break;
			}

			return progress;
		}

		bool _trim()
		{
			/*
 			 * TRIM is not implemented but nonetheless report success
			 * back to client as it merely is a hint.
			 */
			success  = true;
			complete = true;
			return true;
		}

		Job(Vfs::Vfs_handle &handle,
		    Block::Request   request,
		    file_offset      base_offset,
		    char            *data,
		    file_size        length)
		:
			_handle        { handle },
			request        { request },
			data           { data },
			state          { _initial_state(request.operation.type) },
			base_offset    { base_offset },
			current_offset { 0 },
			current_count  { length },
			success        { false },
			complete       { false }
		{ }

		bool completed() const { return complete; }
		bool succeeded()   const { return success; }

		void print(Genode::Output &out) const
		{
			Genode::print(out, "(", request.operation, ")",
				" state: ",          _state_to_string(state),
				" base_offset: ",    base_offset,
				" current_offset: ", current_offset,
				" current_count: ",  current_count,
				" success: ",        success,
				" complete: ",       complete);
		}

		bool execute()
		{
			using Type = Block::Operation::Type;

			switch (request.operation.type) {
			case Type::READ:  return _read();
			case Type::WRITE: return _write();
			case Type::SYNC:  return _sync();
			case Type::TRIM:  return _trim();
			default:          return false;
			}
		}
	};

} /* namespace Vfs_block */

#endif /* _VFS_BLOCK__JOB_ */
