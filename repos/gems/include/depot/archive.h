/*
 * \brief  Utilities to handle depot-archive paths
 * \author Norman Feske
 * \date   2017-12-18
 */

/*
 * Copyright (C) 2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__DEPOT__ARCHIVE_H_
#define _INCLUDE__DEPOT__ARCHIVE_H_

/* Genode includes */
#include <util/string.h>

namespace Depot {
	using namespace Genode;
	struct Archive;
}


struct Depot::Archive
{
	using Path    = String<100>;
	using User    = String<64>;
	using Name    = String<80>;
	using Arch    = String<10>;
	using Version = String<40>;

	enum Type { PKG, RAW, API, SRC, BIN, DBG, IMAGE, INDEX };

	struct Unknown { };

	using User_result     = Attempt<User,    Unknown>;
	using Type_result     = Attempt<Type,    Unknown>;
	using Name_result     = Attempt<Name,    Unknown>;
	using Version_result  = Attempt<Version, Unknown>;
	using Bin_path_result = Attempt<Path,    Unknown>;


	/**
	 * Return Nth path element
	 *
	 * The first path element corresponds to n == 0.
	 */
	template <typename STRING>
	static Attempt<STRING, Unknown> _path_element(Path const &path, unsigned n)
	{
		char const *s = path.string();

		/* skip 'n' path elements */
		for (; n > 0; n--) {

			/* search '/' */
			while (*s && *s != '/')
				s++;

			if (*s == 0)
				return Unknown { };

			/* skip '/' */
			s++;
		}

		/* find '/' marking the end of the path element */
		unsigned i = 0;
		while (s[i] != 0 && s[i] != '/')
			i++;

		if (i == 0)
			return Unknown { };

		return STRING(Cstring(s, i));
	}

	template <typename STRING>
	static bool _path_element_equals(Path const &path, unsigned n, STRING const &match)
	{
		return _path_element<STRING>(path, n).template convert<bool>(
			[&] (STRING const &s) { return s == match; },
			[&] (Unknown) -> bool { return false; });
	}

	/**
	 * Return archive user of depot-local path
	 */
	static User_result user(Path const &path)
	{
		return _path_element<User>(path, 0);
	}

	/**
	 * Return archive type of depot-local path
	 */
	static Type_result type(Path const &path)
	{
		using Name = String<8>;
		return _path_element<Name>(path, 1).convert<Type_result>(
			[&] (Name const &name) -> Type_result {
				if (name == "src")   return SRC;
				if (name == "api")   return API;
				if (name == "pkg")   return PKG;
				if (name == "raw")   return RAW;
				if (name == "bin")   return BIN;
				if (name == "dbg")   return DBG;
				if (name == "image") return IMAGE;
				if (name == "index") return INDEX;
				return Unknown { };
			},
			[&] (Unknown) -> Unknown { return { }; });
	}

	/**
	 * Return true if 'path' refers to an index file
	 */
	static bool index(Path const &path)
	{
		return _path_element_equals<Name>(path, 1, "index");
	}

	/**
	 * Return true if 'path' refers to a system-image index file
	 */
	static bool image_index(Path const &path)
	{
		return _path_element_equals<Name>(path, 1, "image")
		    && _path_element_equals<Name>(path, 2, "index");
	}

	/**
	 * Return true if 'path' refers to a system image
	 */
	static bool image(Path const &path)
	{
		return  _path_element_equals<Name>(path, 1, "image")
		    && !_path_element_equals<Name>(path, 2, "index");
	}

	static Name_result name(Path const &path)
	{
		return type(path).convert<Name_result>([&] (Type t) -> Name_result {
			switch (t) {

			case SRC: case API: case PKG: case RAW: case IMAGE:
				return _path_element<Name>(path, 2);

			case BIN: case DBG:
				return _path_element<Name>(path, 3);

			case INDEX: break;
			}
			return Unknown { };
		},
		[&] (Unknown) -> Unknown { return { }; });
	}

	static Version_result version(Path const &path)
	{
		return type(path).convert<Version_result>([&] (Type t) -> Version_result {
			switch (t) {

			case SRC: case API: case PKG: case RAW:
				return _path_element<Version>(path, 3);

			case INDEX:
				return _path_element<Version>(path, 2);

			case BIN: case DBG:
				return _path_element<Version>(path, 4);

			case IMAGE: break;
			}
			return Unknown { };
		},
		[&] (Unknown) -> Unknown { return { }; });
	}

	/**
	 * Return name of compressed file to download for the given depot path
	 *
	 * Archives are shipped as tar.xz files whereas index files are shipped
	 * as xz-compressed files.
	 */
	static Archive::Path download_file_path(Archive::Path path)
	{
		return (index(path) || image_index(path)) ? Path(path, ".xz")
		                                          : Path(path, ".tar.xz");
	}

	/**
	 * Return path to binary archive for a given src archive
	 */
	static Bin_path_result bin_path(Path const &src, Arch const &arch)
	{
		return user(src).convert<Bin_path_result>([&] (User const &user) {
			return name(src).convert<Bin_path_result>([&] (Name const &name) {
				return version(src).convert<Bin_path_result>([&] (Version const &version) {
					return Path { user, "/bin/", arch, "/", name, "/", version };
				}, [&] (Unknown) -> Unknown { return { }; });
			}, [&] (Unknown) -> Unknown { return { }; });
		}, [&] (Unknown) -> Unknown { return { }; });
	}
};

#endif /* _INCLUDE__DEPOT__ARCHIVE_H_ */
