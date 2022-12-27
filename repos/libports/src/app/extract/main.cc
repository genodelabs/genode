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
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconversion"
#include <fcntl.h>
#include <sys/stat.h>

/* libarchive includes */
#include <archive.h>
#include <archive_entry.h>

#pragma GCC diagnostic pop  /* restore -Wconversion warnings */

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

		/* handle leading '/' */
		if (end_of_elem && (sub_path_len == 0))
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

		using Noncopyable::Noncopyable;

		~Source()
		{
			archive_read_close(ptr);
			archive_read_free(ptr);
		}
	} src { };

	struct Destination : Noncopyable
	{
		archive * const ptr = archive_write_disk_new();

		using Noncopyable::Noncopyable;

		~Destination()
		{
			archive_write_close(ptr);
			archive_write_free(ptr);
		}
	} dst { };

	typedef String<256> Path;
	typedef String<80>  Raw_name;

	struct Exception    : Genode::Exception { };
	struct Open_failed  : Exception { };
	struct Read_failed  : Exception { };
	struct Write_failed : Exception { };

	struct Strip { unsigned value; };

	/**
	 * Constructor
	 *
	 * \param strip     number of leading path elements to strip
	 * \param raw_name  destination file name when uncompressing raw file
	 *
	 * \throw Open_failed
	 * \throw Read_failed
	 * \throw Write_failed
	 *
	 * The 'raw_name' is unused when extracting an archive.
	 */
	explicit Extracted_archive(Path     const &path,
	                           Strip    const strip,
	                           Raw_name const &raw_name)
	{
		archive_read_support_format_all(src.ptr);
		archive_read_support_format_raw(src.ptr);
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

			bool const raw = archive_format(src.ptr) == ARCHIVE_FORMAT_RAW;

			/* set destination file name when uncompressing a raw file */
			if (raw) {
				if (!raw_name.valid()) {
					error("name of uncompressed file for ", path, " not specified");
					throw Write_failed();
				}

				archive_entry_copy_pathname(entry, raw_name.string());
			}

			/* strip leading path elements when extracting an archive */
			if (!raw) {
				auto stripped = [] (char const *name, unsigned const n)
				{
					char const *s = name;

					for (unsigned i = 0; i < n; i++) {

						/* search end of current path element */
						auto end = [] (char c) { return c == '/' || c == 0; };
						while (!end(*s))
							s++;

						/* check if anything is left from the path */
						if (*s == 0 || strcmp(s, "/") == 0)
							return (char const *)nullptr;

						/* skip path delimiter */
						s++;
					}
					return s;
				};

				char const * const stripped_name =
					stripped(archive_entry_pathname(entry), strip.value);

				/* skip archive entry it path is completely stripped away */
				if (stripped_name == nullptr)
					continue;

				archive_entry_copy_pathname(entry, stripped_name);
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

	typedef Extracted_archive::Path     Path;
	typedef Extracted_archive::Raw_name Raw_name;

	Attached_rom_dataspace _config { _env, "config" };

	bool _verbose = false;

	void _process_config()
	{
		Xml_node const config = _config.xml();

		_verbose = config.attribute_value("verbose", false);

		config.for_each_sub_node("extract", [&] (Xml_node node) {

			Path     const src_path = node.attribute_value("archive", Path());
			Path     const dst_path = node.attribute_value("to",      Path());
			Raw_name const raw_name = node.attribute_value("name", Raw_name());

			Extracted_archive::Strip const strip { node.attribute_value("strip", 0U) };

			bool success = false;

			struct Create_directories_failed { };

			try {
				if (!create_directories(dst_path))
					throw Create_directories_failed();

				chdir("/");
				chdir(dst_path.string());

				Extracted_archive extracted_archive(src_path, strip, raw_name);

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

/**
 * Dummy to prevent warning printed by unimplemented libc function
 */
extern "C" mode_t umask(mode_t value) { return value; }

/**
 * Dummy to discharge the dependency from a timer session
 *
 * When libarchive creates a archives, it requests the current time to create
 * up-to-date time stamps. Unfortunately, however, 'time' is called
 * unconditionally regardless of whether an archive is created or extracted.
 * In the latter (our) case, the wall-clock time is not relevant. Still,
 * libarchive creates an artificial dependency from a time source in either
 * case.
 */
extern "C" time_t time(time_t *) { return 0; }

