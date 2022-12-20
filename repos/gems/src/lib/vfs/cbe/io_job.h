/*
 * \brief  Integration of the Consistent Block Encrypter (CBE)
 * \author Martin Stein
 * \author Josef Soentgen
 * \date   2020-11-10
 */

/*
 * Copyright (C) 2020 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _CBE_VFS__IO_JOB_
#define _CBE_VFS__IO_JOB_

namespace Vfs_cbe {

	using file_size = Vfs::file_size;
	using file_offset = Vfs::file_offset;

	struct Io_job
	{
		struct Unsupported_Operation : Genode::Exception { };
		struct Invalid_state         : Genode::Exception { };

		enum State { PENDING, IN_PROGRESS, COMPLETE, };

		static State _initial_state(Cbe::Request::Operation const op)
		{
			using Op = Cbe::Request::Operation;

			switch (op) {
			case Op::READ:  return State::PENDING;
			case Op::WRITE: return State::PENDING;
			case Op::SYNC:  return State::PENDING;
			default:        throw Unsupported_Operation();
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

		Cbe::Request::Operation const _op;
		Cbe::Io_buffer::Index   const _index;
		State                         _state;
		file_offset             const _base_offset;
		file_offset                   _current_offset;
		file_size                     _current_count;

		bool _success;
		bool _complete;

		bool _read(Cbe::Library &cbe, Cbe::Io_buffer &io_data)
		{
			bool progress = false;

			switch (_state) {
			case State::PENDING:

				_handle.seek(_base_offset + _current_offset);
				if (!_handle.fs().queue_read(&_handle, _current_count)) {
					return progress;
				}

				cbe.io_request_in_progress(_index);

				_state = State::IN_PROGRESS;
				progress = true;
			[[fallthrough]];
			case State::IN_PROGRESS:
			{
				using Result = Vfs::File_io_service::Read_result;

				bool completed = false;
				file_size out = 0;

				char * const data = reinterpret_cast<char *const>(&io_data.item(_index));

				Result const result =
					_handle.fs().complete_read(&_handle,
					                           data + _current_offset,
					                           _current_count, out);
				if (result == Result::READ_QUEUED
				 || result == Result::READ_ERR_WOULD_BLOCK) {
					return progress;
				} else

				if (result == Result::READ_OK) {
					_current_offset += out;
					_current_count  -= out;
					_success = true;
				} else

				if (   result == Result::READ_ERR_IO
				    || result == Result::READ_ERR_INVALID) {
					_success   = false;
					completed = true;
				}

				if (_current_count == 0 || completed) {
					_state = State::COMPLETE;
				} else {
					_state = State::PENDING;
					/* partial read, keep trying */
					return true;
				}
				progress = true;
			}
			[[fallthrough]];
			case State::COMPLETE:

				cbe.io_request_completed(_index, _success);
				_complete = true;
				progress = true;
			default: break;
			}

			return progress;
		}
 
		bool _write(Cbe::Library &cbe, Cbe::Io_buffer &io_data)
		{
			bool progress = false;

			switch (_state) {
			case State::PENDING:

				_handle.seek(_base_offset + _current_offset);

				cbe.io_request_in_progress(_index);
				_state = State::IN_PROGRESS;
				progress = true;
			[[fallthrough]];
			case State::IN_PROGRESS:
			{
				using Result = Vfs::File_io_service::Write_result;

				bool completed = false;
				file_size out = 0;

				char const * const data =
					reinterpret_cast<char const * const>(&io_data.item(_index));

				Result const result =
					_handle.fs().write(&_handle, data + _current_offset,
					                   _current_count, out);
				switch (result) {
				case Result::WRITE_ERR_WOULD_BLOCK:
					return progress;

				case Result::WRITE_OK:
					_current_offset += out;
					_current_count  -= out;
					_success = true;
					break;

				case Result::WRITE_ERR_IO:
				case Result::WRITE_ERR_INVALID:
					_success = false;
					completed = true;
					break;
				}

				if (_current_count == 0 || completed) {
					_state = State::COMPLETE;
				} else {
					_state = State::PENDING;
					/* partial write, keep trying */
					return true;
				}
				progress = true;
			}
			[[fallthrough]];
			case State::COMPLETE:

				cbe.io_request_completed(_index, _success);
				_complete = true;
				progress = true;
			default: break;
			}

			return progress;
		}

		bool _sync(Cbe::Library &cbe, Cbe::Io_buffer &io_data)
		{
			bool progress = false;

			switch (_state) {
			case State::PENDING:

				if (!_handle.fs().queue_sync(&_handle)) {
					return progress;
				}
				cbe.io_request_in_progress(_index);
				_state = State::IN_PROGRESS;
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
					_success = false;
				} else

				if (result == Result::SYNC_OK) {
					_success = true;
				}

				_state = State::COMPLETE;
				progress = true;
			}
			[[fallthrough]];
			case State::COMPLETE:

				cbe.io_request_completed(_index, _success);
				_complete = true;
				progress = true;
			default: break;
			}

			return progress;
		}

		Io_job(Vfs::Vfs_handle         &handle,
		       Cbe::Request::Operation  op,
		       Cbe::Io_buffer::Index    index,
		       file_offset              base_offset,
		       file_size                length)
		:
			_handle         { handle },
			_op             { op },
			_index          { index },
			_state          { _initial_state(op) },
			_base_offset    { base_offset },
			_current_offset { 0 },
			_current_count  { length },
			_success        { false },
			_complete       { false }
		{ }

		bool completed() const { return _complete; }
		bool succeeded() const { return _success; }

		void print(Genode::Output &out) const
		{
			Genode::print(out, "(", to_string(_op), ")",
				" state: ",          _state_to_string(_state),
				" base_offset: ",    _base_offset,
				" current_offset: ", _current_offset,
				" current_count: ",  _current_count,
				" success: ",        _success,
				" complete: ",       _complete);
		}

		bool execute(Cbe::Library &cbe, Cbe::Io_buffer &io_data)
		{
			using Op = Cbe::Request::Operation;

			switch (_op) {
			case Op::READ:  return _read(cbe, io_data);
			case Op::WRITE: return _write(cbe, io_data);
			case Op::SYNC:  return _sync(cbe, io_data);
			default:        return false;
			}
		}
	};

} /* namespace Vfs_cbe */

#endif /* _CBE_VFS__IO_JOB_ */
