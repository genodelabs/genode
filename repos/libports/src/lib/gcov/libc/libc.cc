/*
 * \brief  Minimal C library for libgcov
 * \author Christian Prochaska
 * \date   2018-11-16
 *
 */

/*
 * Copyright (C) 2018 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <base/attached_rom_dataspace.h>
#include <base/env.h>
#include <base/heap.h>
#include <base/log.h>
#include <base/sleep.h>
#include <file_system_session/connection.h>
#include <file_system/util.h>
#include <util/string.h>
#include <util/xml_node.h>
#include <format/snprintf.h>

extern "C" {
#include "stdio.h"
}

using namespace Genode;

using Absolute_path = Path<File_system::MAX_PATH_LEN>;

struct FILE { };

static FILE stderr_file;

FILE *stderr = &stderr_file;

struct Gcov_env
{
	using seek_off_t = File_system::seek_off_t;

	Env                    &env;
	Attached_rom_dataspace  config { env, "config" };
	Heap                    heap { env.ram(), env.rm() };
	Allocator_avl           fs_alloc { &heap };

	/*
	 * We use a file-system session directly to exfiltrate the gcov data from
	 * the component without any interplay with the libc or the component's
	 * VFS.
	 */

	File_system::Connection fs { env, fs_alloc, "gcov_data" };
	seek_off_t              seek_offset { 0 };

	Io_signal_handler<Gcov_env> _fs_signal_handler {
		env.ep(), *this, &Gcov_env::_handle_fs_signal };

	void _handle_fs_signal() { }

	/* only one file is open at a time */
	Constructible<File_system::File_handle> file_handle;
	FILE                                    file;

	void _block_for_ack()
	{
		while (!fs.tx()->ack_avail())
			env.ep().wait_and_dispatch_one_io_signal();
	}

	Gcov_env(Env &env)
	:
		env(env)
	{
		fs.sigh(_fs_signal_handler);
	}

	size_t read (File_system::Node_handle const &, void *,       size_t, seek_off_t);
	size_t write(File_system::Node_handle const &, void const *, size_t, seek_off_t);
};


size_t Gcov_env::read(File_system::Node_handle const &node_handle,
                      void *dst, size_t count, seek_off_t seek_offset)
{
	bool success = true;
	File_system::Session::Tx::Source &source = *fs.tx();

	size_t const max_packet_size = source.bulk_buffer_size() / 2;

	size_t remaining_count = count;

	while (remaining_count && success) {

		size_t const curr_packet_size =
			min(remaining_count, max_packet_size);

		File_system::Packet_descriptor
			packet(source.alloc_packet(curr_packet_size),
			       node_handle,
			       File_system::Packet_descriptor::READ,
			       curr_packet_size,
			       seek_offset);

		/* pass packet to server side */
		source.submit_packet(packet);

		_block_for_ack();

		packet = source.get_acked_packet();
		success = packet.succeeded();

		size_t const read_num_bytes =
			min(packet.length(), curr_packet_size);

		/* copy-out payload into destination buffer */
		memcpy(dst, source.packet_content(packet), read_num_bytes);

		source.release_packet(packet);

		/* prepare next iteration */
		seek_offset += read_num_bytes;
		dst = (void *)((addr_t)dst + read_num_bytes);
		remaining_count -= read_num_bytes;

		/*
		 * If we received less bytes than requested, we reached the end
		 * of the file.
		 */
		if (read_num_bytes < curr_packet_size)
			break;
	}

	return count - remaining_count;
}


size_t Gcov_env::write(File_system::Node_handle const &node_handle,
                       void const *src, size_t count,
                       seek_off_t seek_offset)
{
	bool success = true;
	File_system::Session::Tx::Source &source = *fs.tx();

	size_t const max_packet_size = source.bulk_buffer_size() / 2;

	size_t remaining_count = count;

	while (remaining_count && success) {

		size_t const curr_packet_size =
			min(remaining_count, max_packet_size);

		File_system::Packet_descriptor
			packet(source.alloc_packet(curr_packet_size),
			       node_handle,
			       File_system::Packet_descriptor::WRITE,
			       curr_packet_size,
			       seek_offset);

		/* copy-out source buffer into payload */
		memcpy(source.packet_content(packet), src, curr_packet_size);

		/* pass packet to server side */
		source.submit_packet(packet);

		_block_for_ack();

		packet = source.get_acked_packet();;
		success = packet.succeeded();
		source.release_packet(packet);

		/* prepare next iteration */
		seek_offset += curr_packet_size;
		src = (void *)((addr_t)src + curr_packet_size);
		remaining_count -= curr_packet_size;
	}

	return count - remaining_count;
}


static Constructible<Gcov_env> gcov_env;


void gcov_init(Env &env)
{
	gcov_env.construct(env);
}


extern "C" void abort()
{
	error("abort() called: not implemented");
	sleep_forever();
}


extern "C" int atoi(const char *nptr)
{
	error("atoi() called: not implemented");
	return 0;
}


extern "C" void exit(int status)
{
	gcov_env->env.parent().exit(status);
	sleep_forever();
}


extern "C" int fclose(FILE *stream)
{
	gcov_env->fs.close(*(gcov_env->file_handle));
	gcov_env->file_handle.destruct();
	return 0;
}


extern "C" FILE *fopen(const char *path, const char *mode)
{
	Absolute_path dir_path(path);
	dir_path.strip_last_element();

	Absolute_path file_name(path);
	file_name.keep_only_last_element();

	File_system::Dir_handle dir = File_system::ensure_dir(gcov_env->fs, dir_path.base());

	try {
		gcov_env->file_handle.construct(gcov_env->fs.file(dir,
		                                                  file_name.base() + 1,
		                                                  File_system::READ_WRITE,
		                                                  false));
		if (strcmp(mode, "w+b", 3) == 0)
			gcov_env->fs.truncate(*(gcov_env->file_handle), 0);

	} catch (File_system::Lookup_failed) {

		if (strcmp(mode, "w+b", 3) == 0)
			gcov_env->file_handle.construct(gcov_env->fs.file(dir,
			                                                  file_name.base() + 1,
			                                                  File_system::READ_WRITE,
		    	                                              true));
		else
			return nullptr;
	}

	gcov_env->seek_offset = 0;

	/*
	 * Write the list of source files to be annotated by gcov into a '.gcan'
	 * file in the same directory as the '.gcda' file.
	 */

	try {

		Xml_node config(gcov_env->config.local_addr<char>(),
		                gcov_env->config.size());

		Xml_node libgcov_node = config.sub_node("libgcov");

		Absolute_path annotate_file_name { file_name };
		annotate_file_name.remove_trailing('a');
		annotate_file_name.remove_trailing('d');
		annotate_file_name.append("an");

		File_system::File_handle annotate_file_handle {
			gcov_env->fs.file(dir, annotate_file_name.base() + 1,
			                  File_system::WRITE_ONLY, true) };

		File_system::seek_off_t seek_offset = 0;

		libgcov_node.for_each_sub_node("annotate", [&] (Xml_node annotate_node) {

			using Source = String<File_system::MAX_PATH_LEN>;
			Source const source = annotate_node.attribute_value("source", Source());

			seek_offset += gcov_env->write(annotate_file_handle,
			                               source.string(),
			                               strlen(source.string()),
			                               seek_offset);

			seek_offset += gcov_env->write(annotate_file_handle,
			                               "\n",
			                               1,
			                               seek_offset);
		});

		gcov_env->fs.close(annotate_file_handle);
	}
	catch (Xml_node::Nonexistent_sub_node) { }

	return &gcov_env->file;
}


extern "C" int fprintf(FILE *stream, const char *format, ...)
{
	if (stream != stderr) {
		error("fprintf() called: not implemented");
		return 0;
	}

	va_list list;
	va_start(list, format);
	vfprintf(stream, format, list);
	va_end(list);

	return 0;
}


extern "C" size_t fread(void *ptr, size_t size, size_t nmemb, FILE *stream)
{
	size_t bytes_read = gcov_env->read(*(gcov_env->file_handle),
	                                   ptr, size * nmemb,
	                                   gcov_env->seek_offset);

	gcov_env->seek_offset += bytes_read;
	
	if (bytes_read == (size * nmemb))
		return nmemb;
	
	return 0;
}


extern "C" void free(void *ptr)
{
	gcov_env->heap.free(ptr, 0);
}


extern "C" int fseek(FILE *stream, long offset, int whence)
{
	if (whence != 0)
		error("fseek(): unsupported 'whence'");

	gcov_env->seek_offset = offset;

	return 0;
}


extern "C" long ftell(FILE *stream)
{
	return gcov_env->seek_offset;
}


extern "C" size_t fwrite(const void *ptr, size_t size, size_t nmemb, FILE *stream)
{
	if (stream == stderr) {
		log(Cstring((const char*)ptr, size*nmemb));
		return 0;
	}

	size_t bytes_written = gcov_env->write(*(gcov_env->file_handle),
	                                       ptr, size * nmemb,
	                                       gcov_env->seek_offset);

	gcov_env->seek_offset += bytes_written;

	if (bytes_written == (size * nmemb))
		return nmemb;

	return 0;
}


extern "C" char *getenv(const char *name)
{
	return nullptr;
}


extern "C" int getpid()
{
	return 1;
}


extern "C" void *malloc(size_t size)
{
	return gcov_env->heap.try_alloc(size).convert<void *>(
		[&] (void *ptr) { return ptr; },
		[&] (Allocator::Alloc_error) -> void * { return nullptr; });
}


extern "C" int sprintf(char *str, const char *format, ...)
{
	using namespace Format;

	va_list list;
	va_start(list, format);

	String_console sc {str, 1024 };
	sc.vprintf(format, list);

	va_end(list);
	return sc.len();
}


extern "C" char *strcat(char *dest, const char *src)
{
	while (*dest != '\0')
		dest++;

	while (*src != '\0') {
		*dest = *src;
		src++;
		dest++;
	}

	*dest = '\0';

	return dest;
}


extern "C" char *strchr(const char *s, int c)
{
	while (*s != '\0') {
		if (*s == c)
			return (char*)s;
		s++;
	}

	if (c == '\0')
		return (char*)s;

	return nullptr;
}


extern "C" char *strcpy(char *dest, const char *src)
{
	copy_cstring(dest, src, strlen(src) + 1);

	return dest;
}


extern "C" size_t strlen(const char *s)
{
	return Genode::strlen(s);
}


extern "C" int vfprintf(FILE *stream, const char *format, va_list list)
{
	if (stream != stderr)
		return 0;

	using namespace Format;

	char buf[1024] { };
	String_console sc { buf, sizeof(buf) };
	sc.vprintf(format, list);
	log(Cstring(buf));

	return sc.len();
}
