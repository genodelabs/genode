/*
 * \brief  File system stress tester
 * \author Emery Hemingway
 * \date   2015-08-29
 */

/*
 * Copyright (C) 2015 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
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
#include <vfs/file_system_factory.h>
#include <vfs/dir_file_system.h>
#include <timer_session/connection.h>
#include <base/heap.h>
#include <base/attached_rom_dataspace.h>
#include <base/snprintf.h>
#include <base/component.h>
#include <base/log.h>
#include <base/exception.h>

using namespace Genode;

inline void assert_mkdir(Vfs::Directory_service::Mkdir_result r)
{
	typedef Vfs::Directory_service::Mkdir_result Result;

	switch (r) {
	case Result::MKDIR_OK: return;
	case Result::MKDIR_ERR_EXISTS:
		error("MKDIR_ERR_EXISTS"); break;
	case Result::MKDIR_ERR_NO_ENTRY:
		error("MKDIR_ERR_NO_ENTRY"); break;
	case Result::MKDIR_ERR_NO_SPACE:
		error("MKDIR_ERR_NO_SPACE"); break;
	case Result::MKDIR_ERR_NO_PERM:
		error("MKDIR_ERR_NO_PERM"); break;
	case Result::MKDIR_ERR_NAME_TOO_LONG:
		error("MKDIR_ERR_NAME_TOO_LONG"); break;
	}
	throw Exception();
}

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
		assert_mkdir(vfs.mkdir(path.base(), 0));
		++count;
		mkdir_b(depth);
	}

	void mkdir_a(int depth)
	{
		if (++depth > MAX_DEPTH) return;

		size_t path_len = strlen(path.base());

		path.append("/b");
		assert_mkdir(vfs.mkdir(path.base(), 0));
		++count;
		mkdir_b(depth);

		path.base()[path_len] = '\0';

		path.append("/a");
		assert_mkdir(vfs.mkdir(path.base(), 0));
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

	Write_test(Vfs::File_system &vfs, Genode::Allocator &alloc, char const *parent)
	: Stress_test(vfs, alloc, parent)
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
			assert_read(handle->fs().read(handle, tmp, sizeof(tmp), n));
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

	Read_test(Vfs::File_system &vfs, Genode::Allocator &alloc, char const *parent)
	: Stress_test(vfs, alloc, parent)
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
	void empty_dir(char const *path)
	{
		::Path subpath(path);
		subpath.append("/");

		Vfs::Directory_service::Dirent dirent;
		for (Vfs::file_size i = vfs.num_dirent(path); i;) {
			vfs.dirent(path, --i, dirent);
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
	}

	Unlink_test(Vfs::File_system &vfs, Genode::Allocator &alloc, char const *parent)
	: Stress_test(vfs, alloc, parent)
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

	Vfs::Dir_file_system vfs_root(env, heap, config_xml.sub_node("vfs"),
	                              Vfs::global_file_system_factory());
	char path[Vfs::MAX_PATH_LEN];

	MAX_DEPTH = config_xml.attribute_value("depth", 16U);

	unsigned long elapsed_ms;
	Timer::Connection timer;

	/* populate the directory file system at / */
	vfs_root.num_dirent("/");

	size_t initial_consumption = env.ram().used();

	/**************************
	 ** Generate directories **
	 **************************/
	{
		int count = 0;
		log("generating directory surface...");
		elapsed_ms = timer.elapsed_ms();

		for (int i = 0; i < ROOT_TREE_COUNT; ++i) {
			snprintf(path, 3, "/%d", i);
			vfs_root.mkdir(path, 0);
			Mkdir_test test(vfs_root, heap, path);
			count += test.wait();
		}
		elapsed_ms = timer.elapsed_ms() - elapsed_ms;

		vfs_root.sync("/");

		log("created ",count," empty directories, ",
		    (elapsed_ms*1000)/count,"μs/op , ",
		    env.ram().used()/1024,"KB consumed");
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

		vfs_root.sync("/");

		log("created ",count," empty files, ",
		    (elapsed_ms*1000)/count,"μs/op, ",
		    env.ram().used()/1024,"KB consumed");
	}


	/*****************
	 ** Write files **
	 *****************/

	if (!config_xml.attribute_value("write", true)) {
		elapsed_ms = timer.elapsed_ms();
		log("total: ",elapsed_ms,"ms, ",env.ram().used()/1024,"K consumed");
		return die(env, 0);
	}
	{
		Vfs::file_size count = 0;
		log("writing files...");
		elapsed_ms = timer.elapsed_ms();

		for (int i = 0; i < ROOT_TREE_COUNT; ++i) {
			snprintf(path, 3, "/%d", i);
			Write_test test(vfs_root, heap, path);
			count += test.wait();

		}

		elapsed_ms = timer.elapsed_ms() - elapsed_ms;

		vfs_root.sync("/");

		log("wrote ",count," bytes ",
		    count/elapsed_ms,"kB/s, ",
		    env.ram().used()/1024,"KB consumed");
	}


	/*****************
	 ** Read files **
	 *****************/

	if (!config_xml.attribute_value("read", true)) {
		elapsed_ms = timer.elapsed_ms();

		log("total: ",elapsed_ms,"ms, ",env.ram().used()/1024,"KB consumed");
		return die(env, 0);
	}
	{
		Vfs::file_size count = 0;
		log("reading files...");
		elapsed_ms = timer.elapsed_ms();

		for (int i = 0; i < ROOT_TREE_COUNT; ++i) {
			snprintf(path, 3, "/%d", i);
			Read_test test(vfs_root, heap, path);
			count += test.wait();
		}

		elapsed_ms = timer.elapsed_ms() - elapsed_ms;

		vfs_root.sync("/");

		log("read ",count," bytes, ",
		    count/elapsed_ms,"kB/s, ",
		    env.ram().used()/1024,"KB consumed");
	}


	/******************
	 ** Unlink files **
	 ******************/

	if (!config_xml.attribute_value("unlink", true)) {
		elapsed_ms = timer.elapsed_ms();
		log("total: ",elapsed_ms,"ms, ",env.ram().used()/1024,"KB consumed");
		return die(env, 0);

	}
	{
		Vfs::file_size count = 0;

		log("unlink files...");
		elapsed_ms = timer.elapsed_ms();

		for (int i = 0; i < ROOT_TREE_COUNT; ++i) {
			snprintf(path, 3, "/%d", i);
			Unlink_test test(vfs_root, heap, path);
			count += test.wait();

		}

		elapsed_ms = timer.elapsed_ms() - elapsed_ms;

		vfs_root.sync("/");

		log("unlinked ",count," files in ",elapsed_ms,"ms, ",
		    env.ram().used()/1024,"KB consumed");
	}

	log("total: ",timer.elapsed_ms(),"ms, ",
	    env.ram().used()/1024,"KB consumed");

	size_t outstanding = env.ram().used() - initial_consumption;
	if (outstanding) {
		if (outstanding < 1024)
			error(outstanding, "B not freed after unlink and sync!");
		else
			error(outstanding/1024,"KB not freed after unlink and sync!");
	}

	die(env, 0);
}
