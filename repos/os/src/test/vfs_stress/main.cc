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
#include <base/process.h>
#include <os/config.h>
#include <base/printf.h>
#include <base/snprintf.h>
#include <base/exception.h>

using namespace Genode;

inline void assert_mkdir(Vfs::Directory_service::Mkdir_result r)
{
	typedef Vfs::Directory_service::Mkdir_result Result;

	switch (r) {
	case Result::MKDIR_OK: return;
	case Result::MKDIR_ERR_EXISTS:
		PERR("MKDIR_ERR_EXISTS"); break;
	case Result::MKDIR_ERR_NO_ENTRY:
		PERR("MKDIR_ERR_NO_ENTRY"); break;
	case Result::MKDIR_ERR_NO_SPACE:
		PERR("MKDIR_ERR_NO_SPACE"); break;
	case Result::MKDIR_ERR_NO_PERM:
		PERR("MKDIR_ERR_NO_PERM"); break;
	case Result::MKDIR_ERR_NAME_TOO_LONG:
		PERR("MKDIR_ERR_NAME_TOO_LONG"); break;
	}
	throw Exception();
}

inline void assert_open(Vfs::Directory_service::Open_result r)
{
	typedef Vfs::Directory_service::Open_result Result;
	switch (r) {
	case Result::OPEN_OK: return;
	case Result::OPEN_ERR_NAME_TOO_LONG:
		PERR("OPEN_ERR_NAME_TOO_LONG"); break;
	case Result::OPEN_ERR_UNACCESSIBLE:
		PERR("OPEN_ERR_UNACCESSIBLE"); break;
	case Result::OPEN_ERR_NO_SPACE:
		PERR("OPEN_ERR_NO_SPACE"); break;
	case Result::OPEN_ERR_NO_PERM:
		PERR("OPEN_ERR_NO_PERM"); break;
	case Result::OPEN_ERR_EXISTS:
		PERR("OPEN_ERR_EXISTS"); break;
	}
	throw Exception();
}

inline void assert_write(Vfs::File_io_service::Write_result r)
{
	typedef Vfs::File_io_service::Write_result Result;
	switch (r) {
	case Result::WRITE_OK: return;
	case Result::WRITE_ERR_AGAIN:
		PERR("WRITE_ERR_AGAIN"); break;
	case Result::WRITE_ERR_WOULD_BLOCK:
		PERR("WRITE_ERR_WOULD_BLOCK"); break;
	case Result::WRITE_ERR_INVALID:
		PERR("WRITE_ERR_INVALID"); break;
	case Result::WRITE_ERR_IO:
		PERR("WRITE_ERR_IO"); break;
	case Result::WRITE_ERR_INTERRUPT:
		PERR("WRITE_ERR_INTERRUPT"); break;
	}
	throw Exception();
}

inline void assert_read(Vfs::File_io_service::Read_result r)
{
	typedef Vfs::File_io_service::Read_result Result;
	switch (r) {
	case Result::READ_OK: return;
	case Result::READ_ERR_AGAIN:
		PERR("READ_ERR_AGAIN"); break;
	case Result::READ_ERR_WOULD_BLOCK:
		PERR("READ_ERR_WOULD_BLOCK"); break;
	case Result::READ_ERR_INVALID:
		PERR("READ_ERR_INVALID"); break;
	case Result::READ_ERR_IO:
		PERR("READ_ERR_IO"); break;
	case Result::READ_ERR_INTERRUPT:
		PERR("READ_ERR_INTERRUPT"); break;
	}
	throw Exception();
}

inline void assert_unlink(Vfs::Directory_service::Unlink_result r)
{
	typedef Vfs::Directory_service::Unlink_result Result;
	switch (r) {
	case Result::UNLINK_OK: return;
	case Result::UNLINK_ERR_NO_ENTRY:
		PERR("UNLINK_ERR_NO_ENTRY"); break;
	case Result::UNLINK_ERR_NO_PERM:
		PERR("UNLINK_ERR_NO_PERM"); break;
	case Result::UNLINK_ERR_NOT_EMPTY:
		PERR("UNLINK_ERR_NOT_EMPTY"); break;
	}
	throw Exception();
}

static int MAX_DEPTH;

typedef Genode::Path<Vfs::MAX_PATH_LEN> Path;


struct Stress_thread : public Genode::Thread<4*1024*sizeof(Genode::addr_t)>
{
	::Path            path;
	Vfs::file_size    count;
	Vfs::File_system &vfs;

	Stress_thread(Vfs::File_system &vfs, char const *parent, Affinity::Location affinity)
	: Thread(parent), path(parent), count(0), vfs(vfs)
	{
		env()->cpu_session()->affinity(cap(), affinity);
	}
};


struct Mkdir_thread : public Stress_thread
{
	Mkdir_thread(Vfs::File_system &vfs, char const *parent, Affinity::Location affinity)
	: Stress_thread(vfs, parent, affinity) { start(); }

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

	void entry()
	{
		try { mkdir_a(1); } catch (...) {
			PERR("failed at %s after %llu directories", path.base(), count);
		}
	}

	int wait()
	{
		join();
		return count;
	}
};


struct Populate_thread : public Stress_thread
{
	Populate_thread(Vfs::File_system &vfs, char const *parent, Affinity::Location affinity)
	: Stress_thread(vfs, parent, affinity) { start(); }


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
				path.base(), Directory_service::OPEN_MODE_CREATE, &handle));
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
			PERR("bad directory %c at the end of %s", dir_type, path.base());
			throw Exception();
		}
	}

	void entry()
	{
		::Path start_path(path.base());
		try {
			path.append("/a");
			populate(1);

			path.import(start_path.base());
			path.append("/b");
			populate(1);
		} catch (...) {
			PERR("failed at %s after %llu files", path.base(), count);
		}
	}

	int wait()
	{
		join();
		return count;
	}
};


struct Write_thread : public Stress_thread
{
	Write_thread(Vfs::File_system &vfs, char const *parent, Affinity::Location affinity)
	: Stress_thread(vfs, parent, affinity) { start(); }

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
				path.base(), Directory_service::OPEN_MODE_WRONLY, &handle));
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
			PERR("bad directory %c at the end of %s", dir_type, path.base());
			throw Exception();
		}
	}

	void entry()
	{
		size_t path_len = strlen(path.base());
		try {
			path.append("/a");
			write(1);

			path.base()[path_len] = '\0';
			path.append("/b");
			write(1);
		} catch (...) {
			PERR("failed at %s after writing %llu bytes", path.base(), count);
		}
	}

	Vfs::file_size wait()
	{
		join();
		return count;
	}
};


struct Read_thread : public Stress_thread
{
	Read_thread(Vfs::File_system &vfs, char const *parent, Affinity::Location affinity)
	: Stress_thread(vfs, parent, affinity) { start(); }

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
				path.base(), Directory_service::OPEN_MODE_RDONLY, &handle));
			Vfs_handle::Guard guard(handle);

			char tmp[MAX_PATH_LEN];
			file_size n;
			assert_read(handle->fs().read(handle, tmp, sizeof(tmp), n));
			if (strcmp(path.base(), tmp, n))
				PERR("read returned bad data");
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
			PERR("bad directory %c at the end of %s", dir_type, path.base());
			throw Exception();
		}
	}

	void entry()
	{
		size_t path_len = strlen(path.base());
		try {
			path.append("/a");
			read(1);

			path.base()[path_len] = '\0';
			path.append("/b");
			read(1);
		} catch (...) {
			PERR("failed at %s after reading %llu bytes", path.base(), count);
		}
	}

	Vfs::file_size wait()
	{
		join();
		return count;
	}
};


struct Unlink_thread : public Stress_thread
{
	Unlink_thread(Vfs::File_system &vfs, char const *parent, Affinity::Location affinity)
	: Stress_thread(vfs, parent, affinity) { start(); }

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
				PERR("reached the end prematurely");
				throw Exception();

			case Vfs::Directory_service::DIRENT_TYPE_DIRECTORY:
				empty_dir(subpath.base());

			default:
				try {
					assert_unlink(vfs.unlink(subpath.base()));
					++count;
				} catch (...) {
					PERR("unlink %s failed", subpath.base());
					throw;
				}
				subpath.strip_last_element();
			}
		}
	}

	void entry()
	{
		typedef Vfs::Directory_service::Unlink_result Result;
		try {
			Result r = vfs.unlink(path.base());
			switch (r) {
			case Result::UNLINK_ERR_NOT_EMPTY:
				PLOG("recursive unlink not supported");
				empty_dir(path.base());
				r = vfs.unlink(path.base());

			case Result::UNLINK_OK:
				PLOG("recursive unlink supported");
				++count;
				return;

			default: break;
			}
			assert_unlink(r);
		} catch (...) {
			PERR("unlink %s failed", path.base());
		}
	}

	int wait()
	{
		join();
		return count;
	}
};


int main()
{
	/* look for dynamic linker */
	try {
		static Genode::Rom_connection rom("ld.lib.so");
		Genode::Process::dynamic_linker(rom.dataspace());
	} catch (...) { }

	static Vfs::Dir_file_system vfs_root(config()->xml_node().sub_node("vfs"),
	                                     Vfs::global_file_system_factory());
	static char path[Vfs::MAX_PATH_LEN];

	MAX_DEPTH = config()->xml_node().attribute_value("depth", 16U);
	unsigned thread_count =
		config()->xml_node().attribute_value("threads", 6U);

	unsigned long elapsed_ms;
	Timer::Connection timer;

	/* populate the directory file system at / */
	vfs_root.num_dirent("/");

	Affinity::Space space = env()->cpu_session()->affinity_space();

	size_t initial_consumption = env()->ram_session()->used();

	/**************************
	 ** Generate directories **
	 **************************/
	{
		int count = 0;
		Mkdir_thread *threads[thread_count];
		PLOG("generating directory surface...");
		elapsed_ms = timer.elapsed_ms();

		for (size_t i = 0; i < thread_count; ++i) {
			snprintf(path, 3, "/%zu", i);
			vfs_root.mkdir(path, 0);
			threads[i] = new (Genode::env()->heap())
				Mkdir_thread(vfs_root, path, space.location_of_index(i));
		}

		for (size_t i = 0; i < thread_count; ++i) {
			count += threads[i]->wait();
			destroy(Genode::env()->heap(), threads[i]);
		}
		elapsed_ms = timer.elapsed_ms() - elapsed_ms;

		vfs_root.sync("/");

		PINF("created %d empty directories, %luμs/op , %zuKB consumed",
		     count, (elapsed_ms*1000)/count, env()->ram_session()->used()/1024);
	}


	/********************
	 ** Generate files **
	 ********************/
	{
		int count = 0;
		Populate_thread *threads[thread_count];
		PLOG("generating files...");
		elapsed_ms = timer.elapsed_ms();

		for (size_t i = 0; i < thread_count; ++i) {
			snprintf(path, 3, "/%zu", i);
			threads[i] = new (Genode::env()->heap())
				Populate_thread(vfs_root, path, space.location_of_index(i));
		}

		for (size_t i = 0; i < thread_count; ++i) {
			count += threads[i]->wait();
			destroy(Genode::env()->heap(), threads[i]);
		}

		elapsed_ms = timer.elapsed_ms() - elapsed_ms;

		vfs_root.sync("/");

		PINF("created %d empty files, %luμs/op, %zuKB consumed",
		     count, (elapsed_ms*1000)/count, env()->ram_session()->used()/1024);
	}


	/*****************
	 ** Write files **
	 *****************/

	if (!config()->xml_node().attribute_value("write", true)) {
		elapsed_ms = timer.elapsed_ms();

		PINF("total: %lums, %zuK consumed",
		     elapsed_ms, env()->ram_session()->used()/1024);

		return 0;
	}
	{
		Vfs::file_size count = 0;
		Write_thread *threads[thread_count];
		PLOG("writing files...");
		elapsed_ms = timer.elapsed_ms();

		for (size_t i = 0; i < thread_count; ++i) {
			snprintf(path, 3, "/%zu", i);
			threads[i] = new (Genode::env()->heap())
				Write_thread(vfs_root, path, space.location_of_index(i));
		}

		for (size_t i = 0; i < thread_count; ++i) {
			count += threads[i]->wait();
			destroy(Genode::env()->heap(), threads[i]);
		}

		elapsed_ms = timer.elapsed_ms() - elapsed_ms;

		vfs_root.sync("/");

		PINF("wrote %llu bytes %llukB/s, %zuKB consumed",
		     count, count/elapsed_ms, env()->ram_session()->used()/1024);
	}


	/*****************
	 ** Read files **
	 *****************/

	if (!config()->xml_node().attribute_value("read", true)) {
		elapsed_ms = timer.elapsed_ms();

		PINF("total: %lums, %zuKB consumed",
		     elapsed_ms, env()->ram_session()->used()/1024);

		return 0;
	}
	{
		Vfs::file_size count = 0;
		Read_thread *threads[thread_count];
		PLOG("reading files...");
		elapsed_ms = timer.elapsed_ms();

		for (size_t i = 0; i < thread_count; ++i) {
			snprintf(path, 3, "/%zu", i);
			threads[i] = new (Genode::env()->heap())
				Read_thread(vfs_root, path, space.location_of_index(i));
		}

		for (size_t i = 0; i < thread_count; ++i) {
			count += threads[i]->wait();
			destroy(Genode::env()->heap(), threads[i]);
		}

		elapsed_ms = timer.elapsed_ms() - elapsed_ms;

		vfs_root.sync("/");

		PINF("read  %llu bytes, %llukB/s, %zuKB consumed",
		     count, count/elapsed_ms, env()->ram_session()->used()/1024);
	}


	/******************
	 ** Unlink files **
	 ******************/

	if (!config()->xml_node().attribute_value("unlink", true)) {
		elapsed_ms = timer.elapsed_ms();

		PINF("total: %lums, %zuKB consumed",
		     elapsed_ms, env()->ram_session()->used()/1024);

		return 0;
	}
	{
		Vfs::file_size count = 0;

		Unlink_thread *threads[thread_count];
		PLOG("unlink files...");
		elapsed_ms = timer.elapsed_ms();

		for (size_t i = 0; i < thread_count; ++i) {
			snprintf(path, 3, "/%zu", i);
			threads[i] = new (Genode::env()->heap())
				Unlink_thread(vfs_root, path, space.location_of_index(i));
		}

		for (size_t i = 0; i < thread_count; ++i) {
			count += threads[i]->wait();
			destroy(Genode::env()->heap(), threads[i]);
		}

		elapsed_ms = timer.elapsed_ms() - elapsed_ms;

		vfs_root.sync("/");

		PINF("unlinked %llu files in %lums, %zuKB consumed",
		     count, elapsed_ms, env()->ram_session()->used()/1024);
	}

	PINF("total: %lums, %zuKB consumed",
	     timer.elapsed_ms(), env()->ram_session()->used()/1024);

	size_t outstanding = env()->ram_session()->used() - initial_consumption;
	if (outstanding) {
		if (outstanding < 1024)
			PERR("%zuB not freed after unlink and sync!", outstanding);
		else
			PERR("%zuKB not freed after unlink and sync!", outstanding/1024);
	}

	return 0;
}
