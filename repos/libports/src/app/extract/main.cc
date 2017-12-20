/*
 * \brief  Tool for extracting archives
 * \author Norman Feske
 * \date   2017-12-20
 */

/*
 * Copyright (C) 2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <libc/component.h>
#include <base/attached_rom_dataspace.h>

/* libc includes */
#include <fcntl.h>
#include <sys/stat.h>

/* libarchive includes */
#include <archive.h>
#include <archive_entry.h>

namespace Extract {
	using namespace Genode;
	struct Extracted_archive;
	struct Main;
}


/**
 * Create compound directories leading to the path
 *
 * \return true on success
 */
template <Genode::size_t N>
bool create_directories(Genode::String<N> const &path)
{
	using namespace Genode;

	for (size_t sub_path_len = 0; ; sub_path_len++) {

		char const c = path.string()[sub_path_len];

		bool const end_of_path = (c == 0);
		bool const end_of_elem = (c == '/');

		if (!end_of_elem && !end_of_path)
			continue;

		Genode::String<N> sub_path(Genode::Cstring(path.string(), sub_path_len));

		/* create directory for sub path if it does not already exist */
		struct stat sb;
		sb.st_mode = 0;
		stat(sub_path.string(), &sb);

		if (!S_ISDIR(sb.st_mode)) {
			if (mkdir(sub_path.string(), 0777) < 0)
				return false;
		}

		if (end_of_path)
			break;

		/* skip '/' */
		sub_path_len++;
	}
	return true;
}


struct Extract::Extracted_archive : Noncopyable
{
	struct Source : Noncopyable
	{
		archive * const ptr = archive_read_new();

		~Source()
		{
			archive_read_close(ptr);
			archive_read_free(ptr);
		}
	} src { };

	struct Destination : Noncopyable
	{
		archive * const ptr = archive_write_disk_new();

		~Destination()
		{
			archive_write_close(ptr);
			archive_write_free(ptr);
		}
	} dst { };

	typedef String<256> Path;

	struct Exception    : Genode::Exception { };
	struct Open_failed  : Exception { };
	struct Read_failed  : Exception { };
	struct Write_failed : Exception { };

	/**
	 * Constructor
	 *
	 * \throw Open_failed
	 * \throw Read_failed
	 * \throw Write_failed
	 */
	explicit Extracted_archive(Path const &path)
	{
		archive_read_support_format_all(src.ptr);
		archive_read_support_filter_all(src.ptr);

		size_t const block_size = 10240;

		if (archive_read_open_filename(src.ptr, path.string(), block_size))
			throw Open_failed();

		for (;;) {

			struct archive_entry *entry = nullptr;

			{
				int const ret = archive_read_next_header(src.ptr, &entry);

				if (ret == ARCHIVE_EOF)
					break;

				if (ret != ARCHIVE_OK)
					throw Read_failed();
			}

			if (archive_write_header(dst.ptr, entry) != ARCHIVE_OK)
				throw Write_failed();

			for (;;) {
				void const *buf    = nullptr;
				size_t      size   = 0;
				::int64_t   offset = 0;

				{
					int const ret = archive_read_data_block(src.ptr, &buf, &size, &offset);

					if (ret == ARCHIVE_EOF)
						break;

					if (ret != ARCHIVE_OK)
						throw Read_failed();
				}

				if (archive_write_data_block(dst.ptr, buf, size, offset) != ARCHIVE_OK)
					throw Write_failed();
			}

			if (archive_write_finish_entry(dst.ptr) != ARCHIVE_OK)
				throw Write_failed();
		}
	}
};


struct Extract::Main
{
	Env &_env;

	typedef Extracted_archive::Path Path;

	Attached_rom_dataspace _config { _env, "config" };

	bool _verbose = false;

	void _process_config()
	{
		Xml_node const config = _config.xml();

		_verbose = config.attribute_value("verbose", false);

		config.for_each_sub_node("extract", [&] (Xml_node node) {

			Path const src_path = node.attribute_value("archive", Path());
			Path const dst_path = node.attribute_value("to",      Path());

			bool success = false;

			struct Create_directories_failed { };

			try {
				if (!create_directories(dst_path))
					throw Create_directories_failed();

				chdir("/");
				chdir(dst_path.string());

				Extracted_archive extracted_archive(src_path);

				success = true;
			}
			catch (Create_directories_failed) {
				warning("failed to created directory '", dst_path, "'"); }
			catch (Extracted_archive::Read_failed) {
				warning("reading from archive ", src_path, " failed"); }
			catch (Extracted_archive::Open_failed) {
				warning("could not open archive ", src_path); }
			catch (Extracted_archive::Write_failed) {
				warning("writing to directory ", dst_path, " failed"); }

			/* abort on first error */
			if (!success)
				throw Exception();

			if (_verbose)
				log("extracted '", src_path, "' to '", dst_path, "'");
		});
	}

	Main(Env &env) : _env(env)
	{
		Libc::with_libc([&] () { _process_config(); });

		env.parent().exit(0);
	}
};


void Libc::Component::construct(Libc::Env &env)
{
	static Extract::Main main(env);
}

/* dummy to prevent warning printed by unimplemented libc function */
extern "C" mode_t umask(mode_t value) { return value; }

