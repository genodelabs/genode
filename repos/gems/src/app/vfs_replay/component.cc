/*
 * \brief  VFS replay tool
 * \author Josef Soentgen
 * \date   2020-03-18
 */

/*
 * Copyright (C) 2020 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <base/attached_ram_dataspace.h>
#include <base/attached_rom_dataspace.h>
#include <base/component.h>
#include <base/heap.h>

#include <util/string.h>

#include <vfs/simple_env.h>
#include <vfs/file_system_factory.h>
#include <vfs/dir_file_system.h>


using namespace Genode;
using Vfs::file_offset;
using Vfs::file_size;


class Vfs_replay
{
	private:

		Vfs_replay(const Vfs_replay&) = delete;
		Vfs_replay& operator=(const Vfs_replay&) = delete;

		Env &_env;

		Vfs::File_system &_vfs;
		Vfs::Env::Io     &_io;
		Vfs::Vfs_handle  *_vfs_handle;

		Attached_ram_dataspace _write_buffer;
		Attached_ram_dataspace _read_buffer;

		bool _verbose;

		Xml_node _replay_node;
		Xml_node _request_node { "<empty/>" };
		size_t   _num_requests { 0 };
		unsigned _curr_request_id { 0 };

		bool _finished { false };

		struct Request
		{
			enum Type { INVALID, READ, WRITE, SYNC, };

			static char const *type_to_string(Type t)
			{
				switch (t) {
					case Type::INVALID: return "INVALID";
					case Type::READ:    return "READ";
					case Type::WRITE:   return "WRITE";
					case Type::SYNC:    return "SYNC";
				}
				return "<unknown>";
			}

			enum State {
				NONE,
				READ_PENDING,  READ_IN_PROGRESS,  READ_COMPLETE,
				WRITE_PENDING, WRITE_IN_PROGRESS, WRITE_COMPLETE,
				SYNC_PENDING,  SYNC_IN_PROGRESS,  SYNC_COMPLETE,
				ERROR,
			};

			static char const *state_to_string(State s)
			{
				switch (s) {
					case State::NONE:              return "NONE";
					case State::READ_PENDING:      return "READ_PENDING";
					case State::READ_IN_PROGRESS:  return "READ_IN_PROGRESS";
					case State::READ_COMPLETE:     return "READ_COMPLETE";
					case State::WRITE_PENDING:     return "WRITE_PENDING";
					case State::WRITE_IN_PROGRESS: return "WRITE_IN_PROGRESS";
					case State::WRITE_COMPLETE:    return "WRITE_COMPLETE";
					case State::SYNC_PENDING:      return "SYNC_PENDING";
					case State::SYNC_IN_PROGRESS:  return "SYNC_IN_PROGRESS";
					case State::SYNC_COMPLETE:     return "SYNC_COMPLETE";
					case State::ERROR:             return "ERROR";
				}
				return "<unknown>";
			}

			Type        type;
			State       state;
			file_offset offset;
			file_size   count;
			file_size   out_count;

			file_offset current_offset;
			file_size   current_count;

			bool        success;
			bool        complete;

			bool pending() const { return state != NONE; }
			bool idle()    const { return state == NONE; }

			void print(Genode::Output &out) const
			{
				Genode::print(out, "[ type: ",          type_to_string(type),
				                   " state: ",          state_to_string(state),
				                   " offset: ",         offset,
				                   " count: ",          count,
				                   " out_count: ",      out_count,
				                   " current_offset: ", current_offset,
				                   " current_count: ",  current_count,
				                   " success: ",        success,
				                   " complete: ",       complete, " ]");
			}
		};

		static Request::Type string_to_type(char const *string)
		{
			enum { READ_LEN = 4, WRITE_LEN = 5, SYNC_LEN = 4, };

			if (Genode::strcmp(string, "read", READ_LEN) == 0) {
				return Request::Type::READ;
			} else

			if (Genode::strcmp(string, "write", WRITE_LEN) == 0) {
				return Request::Type::WRITE;
			} else

			if (Genode::strcmp(string, "sync", SYNC_LEN) == 0) {
				return Request::Type::SYNC;
			} else

			return Request::Type::INVALID;
		}

		Request _current_request { };

		bool _read(Request &request)
		{
			bool progress = false;

			switch (request.state) {
			case Request::State::NONE:

				if (request.count > _read_buffer.size()) {
					struct Buffer_too_small { };
					throw Buffer_too_small ();
				}

				request.state = Request::State::READ_PENDING;
				progress = true;
			[[fallthrough]];
			case Request::State::READ_PENDING:

				_vfs_handle->seek(request.current_offset);
				if (!_vfs_handle->fs().queue_read(_vfs_handle, request.current_count)) {
					return progress;
				}

				request.state = Request::State::READ_IN_PROGRESS;
				progress = true;
			[[fallthrough]];
			case Request::State::READ_IN_PROGRESS:
			{
				using Result = Vfs::File_io_service::Read_result;

				bool completed = false;
				file_size out = 0;

				Result const result =
					_vfs_handle->fs().complete_read(_vfs_handle,
					                                _read_buffer.local_addr<char>(),
					                                request.current_count, out);
				if (   result == Result::READ_QUEUED
				    || result == Result::READ_ERR_INTERRUPT
				    || result == Result::READ_ERR_AGAIN
				    || result == Result::READ_ERR_WOULD_BLOCK) {
					return progress;
				}

				if (result == Result::READ_OK) {
					request.current_offset += out;
					request.current_count  -= out;
					request.success = true;
				} else

				if (   result == Result::READ_ERR_IO
				    || result == Result::READ_ERR_INVALID) {
					request.success = false;
					completed = true;
				}

				if (request.current_count == 0 || completed) {
					request.state = Request::State::READ_COMPLETE;
				} else {
					request.state = Request::State::READ_PENDING;
					return progress;
				}
				progress = true;
			}
			[[fallthrough]];
			case Request::State::READ_COMPLETE:

				request.state    = Request::State::NONE;
				request.complete = true;
				progress = true;
			default: break;
			}

			return progress;
		}

		bool _write(Request &request)
		{
			bool progress = false;

			switch (request.state) {
			case Request::State::NONE:

				if (request.count > _write_buffer.size()) {
					struct Buffer_too_small { };
					throw Buffer_too_small ();
				}

				request.state = Request::State::WRITE_PENDING;
				progress = true;
			[[fallthrough]];
			case Request::State::WRITE_PENDING:

				_vfs_handle->seek(request.current_offset);

				request.state = Request::State::WRITE_IN_PROGRESS;
				progress = true;
			[[fallthrough]];
			case Request::State::WRITE_IN_PROGRESS:
			{
				using Result = Vfs::File_io_service::Write_result;

				bool completed = false;
				file_size out = 0;

				Result result = Result::WRITE_ERR_INVALID;
				try {
					result = _vfs_handle->fs().write(_vfs_handle,
					                                 _write_buffer.local_addr<char>(),
					                                 request.current_count, out);
				} catch (Vfs::File_io_service::Insufficient_buffer) {
					return progress;
				}
				if (   result == Result::WRITE_ERR_AGAIN
				    || result == Result::WRITE_ERR_INTERRUPT
				    || result == Result::WRITE_ERR_WOULD_BLOCK) {
					return progress;
				}
				if (result == Result::WRITE_OK) {
					request.current_offset += out;
					request.current_count  -= out;
					request.success = true;
				}

				if (   result == Result::WRITE_ERR_IO
				    || result == Result::WRITE_ERR_INVALID) {
					request.success = false;
					completed = true;
				}
				if (request.current_count == 0 || completed) {
					request.state = Request::State::WRITE_COMPLETE;
				} else {
					request.state = Request::State::WRITE_PENDING;
					return progress;
				}
				progress = true;
			}
			[[fallthrough]];
			case Request::State::WRITE_COMPLETE:

				request.state = Request::State::NONE;
				request.complete = true;
				progress = true;
			default: break;
			}

			return progress;
		}

		bool _sync(Request &request)
		{
			bool progress = false;

			switch (request.state) {
			case Request::State::NONE:

				request.state = Request::State::SYNC_PENDING;
				progress = true;
			[[fallthrough]];
			case Request::State::SYNC_PENDING:

				if (!_vfs_handle->fs().queue_sync(_vfs_handle)) {
					return progress;
				}
				request.state = Request::State::SYNC_IN_PROGRESS;
				progress = true;
			[[fallthrough]];
			case Request::State::SYNC_IN_PROGRESS:
			{
				using Result = Vfs::File_io_service::Sync_result;
				Result const result = _vfs_handle->fs().complete_sync(_vfs_handle);

				if (result == Result::SYNC_QUEUED) {
					return progress;
				}

				if (result == Result::SYNC_ERR_INVALID) {
					request.success = false;
				}

				if (result == Result::SYNC_OK) {
					request.success = true;
				}

				request.state = Request::State::SYNC_COMPLETE;
				progress = true;
			}
			[[fallthrough]];
			case Request::State::SYNC_COMPLETE:

				request.state = Request::State::NONE;
				request.complete = true;
				progress = true;
			default: break;
			}

			return progress;
		}

		bool _handle_request(Genode::Xml_node const &node)
		{
			if (!_current_request.pending()) {

				file_offset const offset = node.attribute_value("offset", file_size(~0llu));
				file_size   const count  = node.attribute_value("count",  file_size(~0llu));

				using Type_String = String<16>;
				Type_String   const type_string = node.attribute_value("type", Type_String());
				Request::Type const type        = string_to_type(type_string.string());

				_current_request.type = type;

				if (type != Request::Type::INVALID) {
					_current_request = {
						.type           = type,
						.state          = Request::State::NONE,
						.offset         = offset,
						.count          = count,
						.out_count      = 0,
						.current_offset = offset,
						.current_count  = count,
						.success        = false,
						.complete       = false,
					};
					if (_verbose) {
						log("Next request: id: ", _curr_request_id, " ",
						    _current_request);
					}
				}
			}

			switch (_current_request.type) {
			case Request::Type::READ:
				return _read(_current_request);
			case Request::Type::WRITE:
				return _write(_current_request);
			case Request::Type::SYNC:
				return _sync(_current_request);
			case Request::Type::INVALID:
				_current_request.complete = true;
				return true;
			}
			return false;
		}

		void _process_replay()
		{
			bool failed = false;

			while (true) {

				bool const progress = _handle_request(_request_node);
				if (!progress) { break; }

				if (_current_request.complete) {
					if (_verbose) {
						log("Completed request: ", _current_request);
					}

					if (!_current_request.success) {
						error("current request: ", _current_request, " failed");
						failed    = true;
						_finished = true;
						break;
					}

					try {
						_request_node = _replay_node.sub_node(++_curr_request_id);
					} catch (Xml_node::Nonexistent_sub_node) {
						_finished = true;
						break;
					}
				}
			}

			if (_finished) {
				_env.parent().exit(failed ? 1 : 0);
			}

			_io.commit();
		}

		struct Io_response_handler : Vfs::Io_response_handler
		{
			Genode::Signal_context_capability sigh { };

			void read_ready_response() override { }

			void io_progress_response() override
			{
				if (sigh.valid()) {
					Genode::Signal_transmitter(sigh).submit();
				}
			}
		};

		Io_response_handler _io_response_handler { };

	public:

		Vfs_replay(Env &env, Vfs::File_system &vfs, Vfs::Env::Io &io,
		           Xml_node const & config)
		:
			_env { env },
			_vfs { vfs },
			_io  { io },
			_vfs_handle { nullptr },
			_write_buffer { _env.ram(), _env.rm(),
			                config.attribute_value("write_buffer_size", 1u << 20) },
			_read_buffer { _env.ram(), _env.rm(),
			               config.attribute_value("read_buffer_size", 1u << 20) },
			_verbose { config.attribute_value("verbose", false) },
			_replay_node { config.sub_node("replay") }
		{
			Genode::memset(_write_buffer.local_addr<char>(), 0x55,
			               _write_buffer.size());
		}

		void kick_off(Genode::Allocator &alloc, char const *file,
		              Genode::Signal_context_capability sigh_cap)
		{
			typedef Vfs::Directory_service::Open_result Open_result;

			Open_result res = _vfs.open(file,
			                            Vfs::Directory_service::OPEN_MODE_RDWR,
			                            &_vfs_handle, alloc);
			if (res != Open_result::OPEN_OK) {
				throw Genode::Exception();
			}

			_io_response_handler.sigh = sigh_cap;

			_vfs_handle->handler(&_io_response_handler);

			_current_request = {
				.type           = Request::Type::INVALID,
				.state          = Request::State::NONE,
				.offset         = 0,
				.count          = 0,
				.out_count      = 0,
				.current_offset = 0,
				.current_count  = 0,
				.success        = false,
				.complete       = false,
			};

			_num_requests = _replay_node.num_sub_nodes();
			_request_node = _replay_node.sub_node(_curr_request_id);
			_process_replay();
		}

		void io_progress_response_handler()
		{
			/* ignore any out-standing signal */
			if (_finished) { return; }

			_process_replay();
		}
};


struct Main : private Genode::Entrypoint::Io_progress_handler
{
	Genode::Env  &_env;
	Genode::Heap  _heap { _env.ram(), _env.rm() };

	Genode::Attached_rom_dataspace  _config_rom { _env, "config" };

	Vfs::Simple_env _vfs_env { _env, _heap, _config_rom.xml().sub_node("vfs") };

	Genode::Signal_handler<Main> _reactivate_handler {
		_env.ep(), *this, &Main::handle_io_progress };

	Vfs_replay _replay { _env, _vfs_env.root_dir(), _vfs_env.io(), _config_rom.xml() };

	Main(Genode::Env &env) : _env { env }
	{
		using File_name = Genode::String<64>;
		File_name const file_name =
			_config_rom.xml().attribute_value("file", File_name());
		if (!file_name.valid()) {
			Genode::error("config 'file' attribute invalid");
			throw Genode::Exception();
		}

		_env.ep().register_io_progress_handler(*this);

		_replay.kick_off(_heap, file_name.string(), _reactivate_handler);
	}

	void handle_io_progress() override
	{
		_replay.io_progress_response_handler();
	}
};


void Component::construct(Genode::Env &env)
{
	static Main main(env);
}
