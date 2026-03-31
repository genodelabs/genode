/*
 * \brief  Access to the model and report file systems
 * \author Norman Feske
 * \date   2026-03-31
 */

/*
 * Copyright (C) 2026 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _VFS_H_
#define _VFS_H_

/* Genode includes */
#include <os/vfs.h>
#include <os/hid_edit.h>

/* local includes */
#include <file_handler.h>
#include <types.h>

namespace Sculpt { struct Vfs; }


struct Sculpt::Vfs
{
	Env &_env;

	Allocator &_alloc;

	static constexpr char const * const _config =
		"vfs\n"
		"+ dir report | + fs | label: report -> /\n"
		"+ dir model  | + fs | label: model -> /\n"
		"-";

	Genode::Vfs::Simple_env _vfs_env { _env, _alloc, Node(Span(_config, strlen(_config))) };

	Directory _root { _vfs_env };

	Vfs(Env &env, Allocator &alloc) : _env(env), _alloc(alloc) { }

	using Path = Directory::Path;

	template <typename T>
	struct Handler : File_handler<T>
	{
		Handler(Vfs &m, auto &&... args)
		: File_handler<T>(m._env.ep(), m._alloc, m._root, args...) { }
	};

	size_t _file_size(Path const &path)
	{
		/* model content is supposed to be in the order of kilobytes */
		auto const num_bytes = _root.file_size(path);
		if (num_bytes > 1024*1024ul) {
			warning("refusing to access overly large file ", path);
			return 0;
		}
		return size_t(num_bytes);
	}

	/* may throw */
	void _edit(Path const &path, auto const &fn)
	{
		size_t const file_size = _file_size(path);
		size_t const headroom = 16*1024;

		if (file_size == 0) return;

		/* fill edit buffer with original file content */
		Hid_edit edit { _alloc, file_size + headroom, [&] (Byte_range_ptr const &ptr) {
			Readonly_file file { _root, path };
			if (file.read({ ptr.start, file_size }) != file_size) {
				warning("read size differs from file size of ", path);
				return 0ul;
			}
			return file_size;
		} };

		/* let 'fn' perform edits */
		fn(edit);

		/* write back the result */
		edit.with_result([&] (Span const &s) {
			New_file file { _root, path };
			file.append(s);
		},
		[&] (Buffer_error) { warning("buffer exceeded while editing ", path); },
		[&] (Alloc_error)  { warning("alloc error while editing ",     path); });
	}

	void edit(Path const &path, auto const &fn)
	{
		try { _edit(path, fn); }
		catch (...) { warning("failed to edit '", path, "'"); }
	}

	/* may throw */
	void _copy(Path const &from, Path const &to)
	{
		size_t const file_size = _file_size(from);

		if (file_size == 0) return;

		if (file_size + 512*1024 > _env.pd().avail_ram().value) {
			error("refusing to copy file '", from, "' (", file_size, " bytes)");
			return;
		}

		File_content{ _alloc, _root, from, { file_size } }.bytes(
			[&] (char const *start, size_t num_bytes) {
				New_file file { _root, to };
				file.append({ start, num_bytes }); });
	}

	void copy(Path const &from, Path const &to)
	{
		try { _copy(from, to); }
		catch (...) { warning("failed to copy from '", from, "' to '", to, "'"); }
	}
};

#endif /* _VFS_H_ */
