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
	typedef String<100> Path;
	typedef String<64>  User;
	typedef String<80>  Name;
	typedef String<40>  Version;

	enum Type { PKG, RAW, SRC, BIN, DBG, IMAGE };

	struct Unknown_archive_type : Exception { };

	/**
	 * Return Nth path element
	 *
	 * The first path element corresponds to n == 0.
	 */
	template <typename STRING>
	static STRING _path_element(Path const &path, unsigned n)
	{
		char const *s = path.string();

		/* skip 'n' path elements */
		for (; n > 0; n--) {

			/* search '/' */
			while (*s && *s != '/')
				s++;

			if (*s == 0)
				return STRING();

			/* skip '/' */
			s++;
		}

		/* find '/' marking the end of the path element */
		unsigned i = 0;
		while (s[i] != 0 && s[i] != '/')
			i++;

		return STRING(Cstring(s, i));
	}

	/**
	 * Return archive user of depot-local path
	 */
	static User user(Path const &path) { return _path_element<User>(path, 0); }

	/**
	 * Return archive type of depot-local path
	 *
	 * \throw Unknown_archive_type
	 */
	static Type type(Path const &path)
	{
		typedef String<8> Name;
		Name const name = _path_element<Name>(path, 1);

		if (name == "src")   return SRC;
		if (name == "pkg")   return PKG;
		if (name == "raw")   return RAW;
		if (name == "bin")   return BIN;
		if (name == "dbg")   return DBG;
		if (name == "image") return IMAGE;

		throw Unknown_archive_type();
	}

	/**
	 * Return true if 'path' refers to an index file
	 */
	static bool index(Path const &path)
	{
		return _path_element<Name>(path, 1) == "index";
	}

	/**
	 * Return true if 'path' refers to a system-image index file
	 */
	static bool image_index(Path const &path)
	{
		return _path_element<Name>(path, 1) == "image" && name(path) == "index";
	}

	/**
	 * Return true if 'path' refers to a system image
	 */
	static bool image(Path const &path)
	{
		return _path_element<Name>(path, 1) == "image" && name(path) != "index";
	}

	static Name name (Path const &path)
	{
		if ((type(path) == BIN) || (type(path) == DBG))
			return _path_element<Name>   (path, 3);

		return _path_element<Name>   (path, 2);
	}

	static Version version (Path const &path)
	{
		if ((type(path) == BIN) || (type(path) == DBG))
			return _path_element<Version>(path, 4);

		return _path_element<Version>(path, 3);
	}

	static Version index_version(Path const &path) { return _path_element<Version>(path, 2); }

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
};

#endif /* _INCLUDE__DEPOT__ARCHIVE_H_ */
