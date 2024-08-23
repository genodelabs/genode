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

	using Path     = String<256>;
	using Raw_name = String<80>;

	struct Strip { unsigned value; };

	struct Extract_ok { };
	enum class Extract_error : uint32_t {
		OPEN_FAILED, READ_FAILED, WRITE_FAILED, };
	using Extract_result = Attempt<Extract_ok, Extract_error>;

	/**
	 * \param path      path to archive file
	 * \param strip     number of leading path elements to strip
	 * \param raw_name  destination file name when uncompressing raw file
	 *
	 * The 'raw_name' is unused when extracting an archive.
	 */
	Extract_result extract(Path     const &path,
	                       Strip    const strip,
	                       Raw_name const &raw_name)
	{
		archive_read_support_format_all(src.ptr);
		archive_read_support_format_raw(src.ptr);
		archive_read_support_filter_all(src.ptr);

		size_t const block_size = 10240;

		if (archive_read_open_filename(src.ptr, path.string(), block_size))
			return Extract_error::OPEN_FAILED;

		for (;;) {

			struct archive_entry *entry = nullptr;

			{
				int const ret = archive_read_next_header(src.ptr, &entry);

				if (ret == ARCHIVE_EOF)
					break;

				if (ret != ARCHIVE_OK)
					return Extract_error::READ_FAILED;
			}

			bool const raw = archive_format(src.ptr) == ARCHIVE_FORMAT_RAW;

			/* set destination file name when uncompressing a raw file */
			if (raw) {
				if (!raw_name.valid()) {
					error("name of uncompressed file for ", path, " not specified");
					return Extract_error::WRITE_FAILED;
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
				return Extract_error::WRITE_FAILED;

			for (;;) {
				void const *buf    = nullptr;
				size_t      size   = 0;
				::int64_t   offset = 0;

				{
					int const ret = archive_read_data_block(src.ptr, &buf, &size, &offset);

					if (ret == ARCHIVE_EOF)
						break;

					if (ret != ARCHIVE_OK)
						return Extract_error::READ_FAILED;
				}

				if (archive_write_data_block(dst.ptr, buf, size, offset) != ARCHIVE_OK)
					return Extract_error::WRITE_FAILED;
			}

			if (archive_write_finish_entry(dst.ptr) != ARCHIVE_OK)
				return Extract_error::WRITE_FAILED;
		}

		return Extract_ok();
	}
};


struct Extract::Main
{
	Env &_env;

	using Path     = Extracted_archive::Path;
	using Raw_name = Extracted_archive::Raw_name;

	Attached_rom_dataspace _config { _env, "config" };

	bool _verbose = false;

	bool _ignore_failures = false;

	bool _stop_on_failure = false;

	bool _process_config()
	{
		Xml_node const config = _config.xml();

		_verbose = config.attribute_value("verbose", false);

		_ignore_failures = config.attribute_value("ignore_failures", false);

		_stop_on_failure = config.attribute_value("stop_on_failure", true);

		bool success = true;

		config.for_each_sub_node("extract", [&] (Xml_node node) {

			/* ignore any following archives after one has failed */
			if (!success && _stop_on_failure)
				return;

			Path     const src_path = node.attribute_value("archive", Path());
			Path     const dst_path = node.attribute_value("to",      Path());
			Raw_name const raw_name = node.attribute_value("name", Raw_name());

			Extracted_archive::Strip const strip { node.attribute_value("strip", 0U) };

			if (!create_directories(dst_path)) {
				success = false;

				warning("failed to created directory '", dst_path, "'");
				return;
			}

			chdir("/");
			chdir(dst_path.string());

			Extracted_archive archive;
			archive.extract(src_path, strip, raw_name).with_result(
				[&] (Extracted_archive::Extract_ok) {

					if (_verbose)
						log("extracted '", src_path, "' to '", dst_path, "'");
				},
				[&] (Extracted_archive::Extract_error e) {
					success = false;

					using Error = Extracted_archive::Extract_error;

					switch (e) {
					case Error::OPEN_FAILED:
						warning("could not open archive ", src_path);
						break;
					case Error::READ_FAILED:
						warning("reading from archive ", src_path, " failed");
						break;
					case Error::WRITE_FAILED:
						warning("writing to directory ", dst_path, " failed");
						break;
					}
				}
			);
		});

		return success;
	}

	Main(Env &env) : _env(env)
	{
		bool success = false;

		Libc::with_libc([&] () { success = _process_config(); });

		env.parent().exit(_ignore_failures ? 0
		                                   : success ? 0
		                                             : 1);
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

