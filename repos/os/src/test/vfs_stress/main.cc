/*
 * \brief  File system stress tester
 * \author Emery Hemingway
 * \date   2015-08-29
 */

/*
 * Copyright (C) 2015-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/*
 * This test populates the VFS as follows:
 *
 * A directory is created at root for each thread with
 * sequential names. For each of these directories, a
 * subtree is generated until the maximum path depth
 * is reached at each branch. The subtree is layed out
 * like this:
 *
 * a
 * |\
 * a b
 * |\ \
 * a b b
 * |\ \ \
 * a b b b
 * |\ \ \ \
 * . . . . .
 *
 */

/* Genode includes */
#include <vfs/simple_env.h>
#include <timer_session/connection.h>
#include <base/heap.h>
#include <base/attached_rom_dataspace.h>
#include <base/snprintf.h>
#include <base/component.h>
#include <base/log.h>
#include <base/exception.h>

using namespace Genode;

inline void assert_open(Vfs::Directory_service::Open_result r)
{
	typedef Vfs::Directory_service::Open_result Result;
	switch (r) {
	case Result::OPEN_OK: return;
	case Result::OPEN_ERR_NAME_TOO_LONG:
		error("OPEN_ERR_NAME_TOO_LONG"); break;
	case Result::OPEN_ERR_UNACCESSIBLE:
		error("OPEN_ERR_UNACCESSIBLE"); break;
	case Result::OPEN_ERR_NO_SPACE:
		error("OPEN_ERR_NO_SPACE"); break;
	case Result::OPEN_ERR_NO_PERM:
		error("OPEN_ERR_NO_PERM"); break;
	case Result::OPEN_ERR_EXISTS:
		error("OPEN_ERR_EXISTS"); break;
	case Result::OPEN_ERR_OUT_OF_RAM:
		error("OPEN_ERR_OUT_OF_RAM"); break;
	case Result::OPEN_ERR_OUT_OF_CAPS:
		error("OPEN_ERR_OUT_OF_CAPS"); break;
	}
	throw Exception();
}

inline void assert_opendir(Vfs::Directory_service::Opendir_result r)
{
	typedef Vfs::Directory_service::Opendir_result Result;
	switch (r) {
	case Result::OPENDIR_OK: return;
	case Result::OPENDIR_ERR_LOOKUP_FAILED:
		error("OPENDIR_ERR_LOOKUP_FAILED"); break;
	case Result::OPENDIR_ERR_NAME_TOO_LONG:
		error("OPENDIR_ERR_NAME_TOO_LONG"); break;
	case Result::OPENDIR_ERR_NODE_ALREADY_EXISTS:
		error("OPENDIR_ERR_NODE_ALREADY_EXISTS"); break;
	case Result::OPENDIR_ERR_NO_SPACE:
		error("OPENDIR_ERR_NO_SPACE"); break;
	case Result::OPENDIR_ERR_OUT_OF_RAM:
		error("OPENDIR_ERR_OUT_OF_RAM"); break;
	case Result::OPENDIR_ERR_OUT_OF_CAPS:
		error("OPENDIR_ERR_OUT_OF_CAPS"); break;
	case Result::OPENDIR_ERR_PERMISSION_DENIED:
		error("OPENDIR_ERR_PERMISSION_DENIED"); break;
	}
	throw Exception();
}

inline void assert_write(Vfs::File_io_service::Write_result r)
{
	typedef Vfs::File_io_service::Write_result Result;
	switch (r) {
	case Result::WRITE_OK: return;
	case Result::WRITE_ERR_AGAIN:
		error("WRITE_ERR_AGAIN"); break;
	case Result::WRITE_ERR_WOULD_BLOCK:
		error("WRITE_ERR_WOULD_BLOCK"); break;
	case Result::WRITE_ERR_INVALID:
		error("WRITE_ERR_INVALID"); break;
	case Result::WRITE_ERR_IO:
		error("WRITE_ERR_IO"); break;
	case Result::WRITE_ERR_INTERRUPT:
		error("WRITE_ERR_INTERRUPT"); break;
	}
	throw Exception();
}

inline void assert_read(Vfs::File_io_service::Read_result r)
{
	typedef Vfs::File_io_service::Read_result Result;
	switch (r) {
	case Result::READ_OK: return;
	case Result::READ_QUEUED:
		error("READ_QUEUED"); break;
	case Result::READ_ERR_AGAIN:
		error("READ_ERR_AGAIN"); break;
	case Result::READ_ERR_WOULD_BLOCK:
		error("READ_ERR_WOULD_BLOCK"); break;
	case Result::READ_ERR_INVALID:
		error("READ_ERR_INVALID"); break;
	case Result::READ_ERR_IO:
		error("READ_ERR_IO"); break;
	case Result::READ_ERR_INTERRUPT:
		error("READ_ERR_INTERRUPT"); break;
	}
	throw Exception();
}

inline void assert_unlink(Vfs::Directory_service::Unlink_result r)
{
	typedef Vfs::Directory_service::Unlink_result Result;
	switch (r) {
	case Result::UNLINK_OK: return;
	case Result::UNLINK_ERR_NO_ENTRY:
		error("UNLINK_ERR_NO_ENTRY"); break;
	case Result::UNLINK_ERR_NO_PERM:
		error("UNLINK_ERR_NO_PERM"); break;
	case Result::UNLINK_ERR_NOT_EMPTY:
		error("UNLINK_ERR_NOT_EMPTY"); break;
	}
	throw Exception();
}

static int MAX_DEPTH;

typedef Genode::Path<Vfs::MAX_PATH_LEN> Path;


struct Stress_test
{
	::Path             path;
	Vfs::file_size     count;
	Vfs::File_system  &vfs;
	Genode::Allocator &alloc;

	Stress_test(Vfs::File_system &vfs, Genode::Allocator &alloc, char const *parent)
	: path(parent), count(0), vfs(vfs), alloc(alloc) { }
};


struct Mkdir_test : public Stress_test
{
	void mkdir_b(int depth)
	{
		if (++depth > MAX_DEPTH) return;

		path.append("/b");
		Vfs::Vfs_handle *dir_handle;
		assert_opendir(vfs.opendir(path.base(), true, &dir_handle, alloc));
		dir_handle->close();
		++count;
		mkdir_b(depth);
	}

	void mkdir_a(int depth)
	{
		if (++depth > MAX_DEPTH) return;

		size_t path_len = strlen(path.base());

		Vfs::Vfs_handle *dir_handle;

		path.append("/b");
		assert_opendir(vfs.opendir(path.base(), true, &dir_handle, alloc));
		dir_handle->close();
		++count;
		mkdir_b(depth);

		path.base()[path_len] = '\0';

		path.append("/a");
		assert_opendir(vfs.opendir(path.base(), true, &dir_handle, alloc));
		dir_handle->close();
		++count;
		mkdir_a(depth);
	}

	Mkdir_test(Vfs::File_system &vfs, Genode::Allocator &alloc, char const *parent)
	: Stress_test(vfs, alloc, parent)
	{
		try { mkdir_a(1); } catch (...) {
			error("failed at '", path, "' after ", count, " directories");
		}
	}

	int wait()
	{
		return count;
	}
};


struct Populate_test : public Stress_test
{
	void populate(int depth)
	{
		if (++depth > MAX_DEPTH) return;

		using namespace Vfs;

		size_t path_len = 1+strlen(path.base());
		char dir_type = *(path.base()+(path_len-2));

		path.append("/c");
		{
			Vfs_handle *handle = nullptr;
			assert_open(vfs.open(
				path.base(), Directory_service::OPEN_MODE_CREATE, &handle, alloc));
			Vfs_handle::Guard guard(handle);
			++count;
		}

		switch (dir_type) {
		case 'a':
			path.base()[path_len] = '\0';
			path.append("a");
			populate(depth);

		case 'b':
			path.base()[path_len] = '\0';
			path.append("b");
			populate(depth);
			return;

		default:
			Genode::String<2> dir_name(Genode::Cstring(&dir_type, 1));
			error("bad directory '", dir_name, "' at the end of '", path, "'");
			throw Exception();
		}
	}

	Populate_test(Vfs::File_system &vfs, Genode::Allocator &alloc, char const *parent)
	: Stress_test(vfs, alloc, parent)
	{
		::Path start_path(path.base());
		try {
			path.append("/a");
			populate(1);

			path.import(start_path.base());
			path.append("/b");
			populate(1);
		} catch (...) {
			error("failed at '", path, "' after ", count, " files");
		}
	}

	int wait()
	{
		return count;
	}
};


struct Write_test : public Stress_test
{
	Genode::Entrypoint &_ep;

	void write(int depth)
	{
		if (++depth > MAX_DEPTH) return;

		size_t path_len = 1+strlen(path.base());
		char dir_type = *(path.base()+(path_len-2));

		using namespace Vfs;

		path.append("/c");
		{
			Vfs_handle *handle = nullptr;
			assert_open(vfs.open(
				path.base(), Directory_service::OPEN_MODE_WRONLY, &handle, alloc));
			Vfs_handle::Guard guard(handle);

			file_size n;
			assert_write(handle->fs().write(
				handle, path.base(), path_len, n));
			handle->fs().queue_sync(handle);
			while (handle->fs().complete_sync(handle) ==
			       Vfs::File_io_service::SYNC_QUEUED)
				_ep.wait_and_dispatch_one_io_signal();
			count += n;
		}

		switch (dir_type) {
		case 'a':
			path.base()[path_len] = '\0';
			path.append("a");
			write(depth);

		case 'b':
			path.base()[path_len] = '\0';
			path.append("b");
			write(depth);
			return;

		default:
			Genode::String<2> dir_name(Genode::Cstring(&dir_type, 1));
			error("bad directory ",dir_name," at the end of '", path, "'");
			throw Exception();
		}
	}

	Write_test(Vfs::File_system &vfs, Genode::Allocator &alloc,
	           char const *parent, Genode::Entrypoint &ep)
	: Stress_test(vfs, alloc, parent), _ep(ep)
	{
		size_t path_len = strlen(path.base());
		try {
			path.append("/a");
			write(1);

			path.base()[path_len] = '\0';
			path.append("/b");
			write(1);
		} catch (...) {
			error("failed at ",path," after writing ",count," bytes");
		}
	}

	Vfs::file_size wait()
	{
		return count;
	}
};


struct Read_test : public Stress_test
{
	Genode::Entrypoint &_ep;

	void read(int depth)
	{
		if (++depth > MAX_DEPTH) return;

		size_t path_len = 1+strlen(path.base());
		char dir_type = *(path.base()+(path_len-2));

		using namespace Vfs;

		path.append("/c");
		{
			Vfs_handle *handle = nullptr;
			assert_open(vfs.open(
				path.base(), Directory_service::OPEN_MODE_RDONLY, &handle, alloc));
			Vfs_handle::Guard guard(handle);

			char tmp[MAX_PATH_LEN];
			file_size n;
			handle->fs().queue_read(handle, sizeof(tmp));

			Vfs::File_io_service::Read_result read_result;

			while ((read_result =
			        handle->fs().complete_read(handle, tmp, sizeof(tmp), n)) ==
			       Vfs::File_io_service::READ_QUEUED)
				_ep.wait_and_dispatch_one_io_signal();

			assert_read(read_result);

			if (strcmp(path.base(), tmp, n))
				error("read returned bad data");
			count += n;
		}

		switch (dir_type) {
		case 'a':
			path.base()[path_len] = '\0';
			path.append("a");
			read(depth);

		case 'b':
			path.base()[path_len] = '\0';
			path.append("/b");
			read(depth);
			return;

		default:
			Genode::String<2> dir_name(Genode::Cstring(&dir_type, 1));
			error("bad directory ",dir_name," at the end of '", path, "'");
			throw Exception();
		}
	}

	Read_test(Vfs::File_system &vfs, Genode::Allocator &alloc, char const *parent,
	          Genode::Entrypoint &ep)
	: Stress_test(vfs, alloc, parent), _ep(ep)
	{
		size_t path_len = strlen(path.base());
		try {
			path.append("/a");
			read(1);

			path.base()[path_len] = '\0';
			path.append("/b");
			read(1);
		} catch (...) {
			error("failed at ",path," after reading ",count," bytes");
		}
	}

	Vfs::file_size wait()
	{
		return count;
	}
};


struct Unlink_test : public Stress_test
{
	Genode::Entrypoint &_ep;

	void empty_dir(char const *path)
	{
		::Path subpath(path);
		subpath.append("/");

		Vfs::Vfs_handle *dir_handle;
		assert_opendir(vfs.opendir(path, false, &dir_handle, alloc));

		Vfs::Directory_service::Dirent dirent;
		for (Vfs::file_size i = vfs.num_dirent(path); i;) {
			dir_handle->seek(--i * sizeof(dirent));
			dir_handle->fs().queue_read(dir_handle, sizeof(dirent));
			Vfs::file_size out_count;

			while (dir_handle->fs().complete_read(dir_handle, (char*)&dirent,
			                                      sizeof(dirent), out_count) ==
			       Vfs::File_io_service::READ_QUEUED)
				_ep.wait_and_dispatch_one_io_signal();

			subpath.append(dirent.name);
			switch (dirent.type) {
			case Vfs::Directory_service::DIRENT_TYPE_END:
				error("reached the end prematurely");
				throw Exception();

			case Vfs::Directory_service::DIRENT_TYPE_DIRECTORY:
				empty_dir(subpath.base());

			default:
				try {
					assert_unlink(vfs.unlink(subpath.base()));
					++count;
				} catch (...) {
					error("unlink ", subpath," failed");
					throw;
				}
				subpath.strip_last_element();
			}
		}

		dir_handle->close();
	}

	Unlink_test(Vfs::File_system &vfs, Genode::Allocator &alloc,
	            char const *parent, Genode::Entrypoint &ep)
	: Stress_test(vfs, alloc, parent), _ep(ep)
	{
		typedef Vfs::Directory_service::Unlink_result Result;
		try {
			Result r = vfs.unlink(path.base());
			switch (r) {
			case Result::UNLINK_ERR_NOT_EMPTY:
				log("recursive unlink not supported");
				empty_dir(path.base());
				r = vfs.unlink(path.base());

			case Result::UNLINK_OK:
				log("recursive unlink supported");
				++count;
				return;

			default: break;
			}
			assert_unlink(r);
		} catch (...) {
			error("unlink ",path," failed");
		}
	}

	int wait()
	{
		return count;
	}
};

void die(Genode::Env &env, int code) { env.parent().exit(code); }

void Component::construct(Genode::Env &env)
{
	enum { ROOT_TREE_COUNT = 6 };

	Genode::Heap heap(env.ram(), env.rm());

	Attached_rom_dataspace config_rom(env, "config");
	Xml_node const config_xml = config_rom.xml();

	Vfs::Simple_env vfs_env { env, heap, config_xml.sub_node("vfs") };

	Vfs::File_system &vfs_root = vfs_env.root_dir();

	Vfs::Vfs_handle *vfs_root_handle;
	vfs_root.opendir("/", false, &vfs_root_handle, heap);

	auto vfs_root_sync = [&] ()
	{
		while (!vfs_root_handle->fs().queue_sync(vfs_root_handle))
			env.ep().wait_and_dispatch_one_io_signal();

		while (vfs_root_handle->fs().complete_sync(vfs_root_handle) ==
		       Vfs::File_io_service::SYNC_QUEUED)
			env.ep().wait_and_dispatch_one_io_signal();
	};

	char path[Vfs::MAX_PATH_LEN];

	MAX_DEPTH = config_xml.attribute_value("depth", 16U);

	unsigned long elapsed_ms;
	Timer::Connection timer(env);

	/* populate the directory file system at / */
	vfs_root.num_dirent("/");

	size_t initial_consumption = env.ram().used_ram().value;

	/**************************
	 ** Generate directories **
	 **************************/
	{
		int count = 0;
		log("generating directory surface...");
		elapsed_ms = timer.elapsed_ms();

		for (int i = 0; i < ROOT_TREE_COUNT; ++i) {
			snprintf(path, 3, "/%d", i);
			Vfs::Vfs_handle *dir_handle;
			vfs_root.opendir(path, true, &dir_handle, heap);
			dir_handle->close();
			Mkdir_test test(vfs_root, heap, path);
			count += test.wait();
		}
		elapsed_ms = timer.elapsed_ms() - elapsed_ms;

		vfs_root_sync();

		log("created ",count," empty directories, ",
		    (elapsed_ms*1000)/count,"μs/op , ",
		    env.ram().used_ram().value/1024,"KiB consumed");
	}


	/********************
	 ** Generate files **
	 ********************/
	{
		int count = 0;
		log("generating files...");
		elapsed_ms = timer.elapsed_ms();

		for (int i = 0; i < ROOT_TREE_COUNT; ++i) {
			snprintf(path, 3, "/%d", i);
			Populate_test test(vfs_root, heap, path);
			count += test.wait();
		}

		elapsed_ms = timer.elapsed_ms() - elapsed_ms;

		vfs_root_sync();

		log("created ",count," empty files, ",
		    (elapsed_ms*1000)/count,"μs/op, ",
		    env.ram().used_ram().value/1024,"KiB consumed");
	}


	/*****************
	 ** Write files **
	 *****************/

	if (!config_xml.attribute_value("write", true)) {
		elapsed_ms = timer.elapsed_ms();
		log("total: ",elapsed_ms,"ms, ",env.ram().used_ram().value/1024,"K consumed");
		return die(env, 0);
	}
	{
		Vfs::file_size count = 0;
		log("writing files...");
		elapsed_ms = timer.elapsed_ms();

		for (int i = 0; i < ROOT_TREE_COUNT; ++i) {
			snprintf(path, 3, "/%d", i);
			Write_test test(vfs_root, heap, path, env.ep());
			count += test.wait();

		}

		elapsed_ms = timer.elapsed_ms() - elapsed_ms;

		vfs_root_sync();

		log("wrote ",count," bytes ",
		    count/elapsed_ms,"kB/s, ",
		    env.ram().used_ram().value/1024,"KiB consumed");
	}


	/*****************
	 ** Read files **
	 *****************/

	if (!config_xml.attribute_value("read", true)) {
		elapsed_ms = timer.elapsed_ms();

		log("total: ",elapsed_ms,"ms, ",env.ram().used_ram().value/1024,"KiB consumed");
		return die(env, 0);
	}
	{
		Vfs::file_size count = 0;
		log("reading files...");
		elapsed_ms = timer.elapsed_ms();

		for (int i = 0; i < ROOT_TREE_COUNT; ++i) {
			snprintf(path, 3, "/%d", i);
			Read_test test(vfs_root, heap, path, env.ep());
			count += test.wait();
		}

		elapsed_ms = timer.elapsed_ms() - elapsed_ms;

		vfs_root_sync();

		log("read ",count," bytes, ",
		    count/elapsed_ms,"kB/s, ",
		    env.ram().used_ram().value/1024,"KiB consumed");
	}


	/******************
	 ** Unlink files **
	 ******************/

	if (!config_xml.attribute_value("unlink", true)) {
		elapsed_ms = timer.elapsed_ms();
		log("total: ",elapsed_ms,"ms, ",env.ram().used_ram().value/1024,"KiB consumed");
		return die(env, 0);

	}
	{
		Vfs::file_size count = 0;

		log("unlink files...");
		elapsed_ms = timer.elapsed_ms();

		for (int i = 0; i < ROOT_TREE_COUNT; ++i) {
			snprintf(path, 3, "/%d", i);
			Unlink_test test(vfs_root, heap, path, env.ep());
			count += test.wait();

		}

		elapsed_ms = timer.elapsed_ms() - elapsed_ms;

		vfs_root_sync();

		log("unlinked ",count," files in ",elapsed_ms,"ms, ",
		    env.ram().used_ram().value/1024,"KiB consumed");
	}

	log("total: ",timer.elapsed_ms(),"ms, ",
	    env.ram().used_ram().value/1024,"KiB consumed");

	size_t outstanding = env.ram().used_ram().value - initial_consumption;
	if (outstanding) {
		if (outstanding < 1024)
			error(outstanding, "B not freed after unlink and sync!");
		else
			error(outstanding/1024,"KiB not freed after unlink and sync!");
	}

	die(env, 0);
}
