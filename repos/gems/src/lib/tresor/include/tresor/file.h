/*
 * \brief  Tresor-local utilities for accessing VFS files
 * \author Martin Stein
 * \date   2020-10-29
 */

/*
 * Copyright (C) 2023 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _TRESOR__FILE_H_
#define _TRESOR__FILE_H_

/* base includes */
#include <util/string.h>

/* os includes */
#include <vfs/vfs_handle.h>
#include <vfs/simple_env.h>

/* tresor includes */
#include <tresor/assertion.h>

namespace Tresor {

	using namespace Genode;

	using Path = String<128>;

	template <typename> class File;
	template <typename> class Read_write_file;
	template <typename> class Write_only_file;

	inline Vfs::Vfs_handle &open_file(Vfs::Env &env, Tresor::Path const &path, Vfs::Directory_service::Open_mode mode)
	{
		using Open_result = Vfs::Directory_service::Open_result;
		Vfs::Vfs_handle *handle { nullptr };
		ASSERT(env.root_dir().open(path.string(), mode, &handle, env.alloc()) == Open_result::OPEN_OK);
		return *handle;
	}
}


template <typename HOST_STATE>
class Tresor::File
{
	private:

		using Read_result = Vfs::File_io_service::Read_result;
		using Write_result = Vfs::File_io_service::Write_result;
		using Sync_result = Vfs::File_io_service::Sync_result;

		enum State { IDLE, SYNC_QUEUED, READ_QUEUED, READ_INITIALIZED, WRITE_INITIALIZED, WRITE_OFFSET_APPLIED };

		Vfs::Env *_env { };
		Tresor::Path const *_path { };
		HOST_STATE &_host_state;
		State _state { IDLE };
		Vfs::Vfs_handle &_handle;
		Vfs::file_size _num_processed_bytes { 0 };

		/*
		 * Noncopyable
		 */
		File(File const &) = delete;
		File &operator = (File const &) = delete;

	public:

		File(HOST_STATE &host_state, Vfs::Vfs_handle &handle) : _host_state(host_state), _handle(handle) { }

		File(HOST_STATE &host_state, Vfs::Env &env, Tresor::Path const &path, Vfs::Directory_service::Open_mode mode)
		: _env(&env), _path(&path), _host_state(host_state), _handle(open_file(*_env, *_path, mode)) { }

		~File()
		{
			ASSERT(_state == IDLE);
			if (_env)
				_env->root_dir().close(&_handle);
		}

		void read(HOST_STATE succeeded, HOST_STATE failed, Vfs::file_offset off, Byte_range_ptr dst, bool &progress)
		{
			switch (_state) {
			case IDLE:

				_num_processed_bytes = 0;
				_state = READ_INITIALIZED;
				progress = true;
				break;

			case READ_INITIALIZED:

				_handle.seek(off + _num_processed_bytes);
				if (!_handle.fs().queue_read(&_handle, dst.num_bytes - _num_processed_bytes))
					break;

				_state = READ_QUEUED;
				progress = true;
				break;

			case READ_QUEUED:
			{
				size_t num_read_bytes { 0 };
				Byte_range_ptr curr_dst { dst.start + _num_processed_bytes, dst.num_bytes - _num_processed_bytes };
				switch (_handle.fs().complete_read(&_handle, curr_dst, num_read_bytes)) {
				case Read_result::READ_QUEUED:
				case Read_result::READ_ERR_WOULD_BLOCK: break;
				case Read_result::READ_OK:

					_num_processed_bytes += num_read_bytes;
					if (_num_processed_bytes < dst.num_bytes) {
						_state = READ_INITIALIZED;
						progress = true;
						break;
					}
					ASSERT(_num_processed_bytes == dst.num_bytes);
					_state = IDLE;
					_host_state = succeeded;
					progress = true;
					break;

				default:

					error("file: read failed");
					_host_state = failed;
					_state = IDLE;
					progress = true;
					break;
				}
				break;
			}
			default: ASSERT_NEVER_REACHED;
			}
		}

		void write(HOST_STATE succeeded, HOST_STATE failed, Vfs::file_offset off, Const_byte_range_ptr src, bool &progress)
		{
			switch (_state) {
			case IDLE:

				_num_processed_bytes = 0;
				_state = WRITE_INITIALIZED;
				progress = true;
				break;

			case WRITE_INITIALIZED:

				_handle.seek(off + _num_processed_bytes);
				_state = WRITE_OFFSET_APPLIED;
				progress = true;
				break;

			case WRITE_OFFSET_APPLIED:
			{
				size_t num_written_bytes { 0 };
				Const_byte_range_ptr curr_src { src.start + _num_processed_bytes, src.num_bytes - _num_processed_bytes };
				switch (_handle.fs().write(&_handle, curr_src, num_written_bytes)) {
				case Write_result::WRITE_ERR_WOULD_BLOCK: break;
				case Write_result::WRITE_OK:

					_num_processed_bytes += num_written_bytes;
					if (_num_processed_bytes < src.num_bytes) {
						_state = WRITE_INITIALIZED;
						progress = true;
						break;
					}
					ASSERT(_num_processed_bytes == src.num_bytes);
					_state = IDLE;
					_host_state = succeeded;
					progress = true;
					break;

				default:

					error("file: write failed");
					_host_state = failed;
					_state = IDLE;
					progress = true;
					break;
				}
				break;
			}
			default: ASSERT_NEVER_REACHED;
			}
		}

		void sync(HOST_STATE succeeded, HOST_STATE failed, bool &progress)
		{
			switch (_state) {
			case IDLE:

				if (!_handle.fs().queue_sync(&_handle))
					break;

				_state = SYNC_QUEUED;
				progress = true;
				break;

			case SYNC_QUEUED:

				switch (_handle.fs().complete_sync(&_handle)) {
				case Sync_result::SYNC_QUEUED: break;
				case Sync_result::SYNC_OK:

					_state = IDLE;
					_host_state = succeeded;
					progress = true;
					break;

				default:

					error("file: sync failed");
					_host_state = failed;
					_state = IDLE;
					progress = true;
					break;
				}
			default: break;
			}
		}
};

template <typename HOST_STATE>
struct Tresor::Read_write_file : public File<HOST_STATE>
{
	Read_write_file(HOST_STATE &host_state, Vfs::Env &env, Tresor::Path const &path)
	: File<HOST_STATE>(host_state, env, path, Vfs::Directory_service::OPEN_MODE_RDWR) { }
};

template <typename HOST_STATE>
struct Tresor::Write_only_file : public File<HOST_STATE>
{
	Write_only_file(HOST_STATE &host_state, Vfs::Env &env, Tresor::Path const &path)
	: File<HOST_STATE>(host_state, env, path, Vfs::Directory_service::OPEN_MODE_WRONLY) { }
};

#endif /* _TRESOR__FILE_H_ */
