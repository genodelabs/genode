/*
 * \brief  Utilities
 * \author Norman Feske
 * \date   2012-04-11
 */

/*
 * Copyright (C) 2012-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _FILE_SYSTEM__UTIL_H_
#define _FILE_SYSTEM__UTIL_H_

#include <file_system_session/file_system_session.h>
#include <os/path.h>

namespace File_system {

	/**
	 * Return true if character 'c' occurs in null-terminated string 'str'
	*/
	inline bool string_contains(char const *str, char c)
	{
		for (; *str; str++)
			if (*str == c)
				return true;
		return false;
	}

	/**
	 * Return base-name portion of null-terminated path string
	 */
	static inline char const *basename(char const *path)
	{
		char const *start = path;

		for (; *path; path++)
			if (*path == '/')
				start = path + 1;

		return start;
	}


	/**
	 * Return true if specified path contains at least path delimiters
	 */
	static inline bool contains_path_delimiter(char const *path)
	{
		for (; *path; path++)
			if (*path == '/')
				return true;

		return false;
	}


	/**
	 * Return true if 'str' is a valid node name
	 */
	static inline bool valid_name(char const *str)
	{
		if (string_contains(str, '/')) return false;

		/* must have at least one character */
		if (str[0] == 0) return false;

		return true;
	}


	/**
	 * Open a directory, ensuring all parent directories exists.
	 */
	static inline Dir_handle ensure_dir(Session &fs, char const *path)
	{
		try {
			return fs.dir(path, false);
		} catch (Lookup_failed) {
			try {
				return fs.dir(path, true);
			} catch (Lookup_failed) {
				Genode::Path<MAX_PATH_LEN> target(path);
				target.strip_last_element();
				fs.close(ensure_dir(fs, target.base()));
			}
		}
		return fs.dir(path, true);
	}


	class Handle_guard
	{
		private:

			Session     &_session;
			Node_handle  _handle;

		public:

			Handle_guard(Session &session, Node_handle handle)
			: _session(session), _handle(handle) { }

			~Handle_guard() { _session.close(_handle); }
	};

	class Packet_guard
	{
		private:

			Session::Tx::Source           &_source;
			File_system::Packet_descriptor _packet;

		public:

			Packet_guard(Session::Tx::Source           &source,
			             File_system::Packet_descriptor packet)
			: _source(source), _packet(packet) { }

			~Packet_guard()
			{
				_source.release_packet(_packet);
			}
	};

}

#endif /* _FILE_SYSTEM__UTIL_H_ */
