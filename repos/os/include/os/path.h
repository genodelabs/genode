/*
 * \brief  Path handling utility
 * \author Norman Feske
 * \date   2011-02-17
 */

/*
 * Copyright (C) 2011-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__OS__PATH_H_
#define _INCLUDE__OS__PATH_H_

/* Genode includes */
#include <util/string.h>
#include <base/output.h>

namespace Genode {

	class Path_base;
	template <unsigned> class Path;
}


class Genode::Path_base
{
	public:

		class Path_too_long { };
		class Path_invalid  { };

	protected:

		static bool absolute(char const *path)
		{
			return path[0] == '/';
		}

		static bool ends_with(char c, char const *path)
		{
			return path[0] && (path[strlen(path) - 1] == c);
		}

		static void remove_char(char *buf)
		{
			for (; *buf; buf++)
				buf[0] = buf[1];
		}

		static void remove_trailing(char c, char *path)
		{
			int i = 0;
			while (path[i] && path[i + 1]) i++;

			/*
			 * Never touch the first character to preserve the invariant of
			 * the leading slash.
			 */
			if (i > 0 && path[i] == c)
				path[i] = 0;
		}

		template <typename T>
		static T *last_element(T *path)
		{
			T *result = path;
			for (; *path; path++)
				if (path[0] == '/' && path[1] != 0)
					result = path;
			return result;
		}

		static bool empty(char const *path)
		{
			return strlen(path) == 0;
		}

		/**
		 * Remove superfluous single dots followed by a slash from path
		 */
		static void strip_superfluous_dotslashes(char *path)
		{
			for (; *path; path++) {
				if (path[0] != '/') continue;

				/* strip superfluous dots, e.g., "/abs/./path/" -> "/abs/path/" */
				while (path[1] == '.' && path[2] == '/') {
					remove_char(path);
					remove_char(path);
				}
			}
		}

		static void strip_superfluous_slashes(char *path)
		{
			for (; *path; path++) {
				if (path[0] != '/') continue;

				/* strip superfluous slashes, e.g., "//path/" -> "/path/" */
				while (path[1] == '/')
					remove_char(path);
			}
		}

		/**
		 * Find double-dot path element
		 *
		 * \return index of the first dot of the found path element, or
		 *         0 if no double-dot path element could be found.
		 */
		static unsigned find_double_dot_dir(char *path)
		{
			for (unsigned i = 0; path[i]; i++)
				if (path[i] == '/' && path[i + 1] == '.' && path[i + 2] == '.'
				 && (path[i + 3] == 0 || path[i + 3] == '/'))
					return i + 1;

			return 0;
		}

		static void strip(char *dst, size_t count)
		{
			for (char const *src = dst + count; *src; )
				*dst++ = *src++;
			*dst = 0;
		}

		static void strip_double_dot_dirs(char *path)
		{
			unsigned i;
			while ((i = find_double_dot_dir(path))) {

				/* skip slash prepending the double dot */
				unsigned cut_start = i - 1;
				unsigned cut_end   = i + 2;

				/* skip previous path element */
				while (cut_start > 0 && path[cut_start - 1] != '/')
					cut_start--;

				/* skip slash in front of the pair of dots */
				if (cut_start > 0)
					cut_start--;

				strip(path + cut_start, cut_end - cut_start);
			}
		}

	private:

		Path_base(const Path_base &);

		char * const _path;
		size_t const _path_max_len;

		/**
		 * Append 'path'
		 *
		 * \throw Path_too_long
		 */
		void _append(char const *path)
		{
			size_t const orig_len = strlen(_path);

			if (strlen(path) + orig_len + 1 >= _path_max_len)
				throw Path_too_long();

			if (_path)
				copy_cstring(_path + orig_len, path, _path_max_len - orig_len);
		}

		void _append_slash_if_needed()
		{
			if (!ends_with('/', _path))
				_append("/");
		}

		void _strip_from_begin(size_t count) { strip(_path, count); }

	protected:

		/**
		 * Remove superfluous artifacts from absolute path
		 */
		void _canonicalize()
		{
			strip_superfluous_slashes(_path);
			strip_superfluous_dotslashes(_path);
			strip_double_dot_dirs(_path);
			remove_trailing('.', _path);
		}

	public:

		void import(char const *path, char const *pwd = 0)
		{
			if (!path)
				throw Path_invalid();

			/*
			 * Validate 'pwd' argument, if not supplied, enforce invariant
			 * that 'pwd' is an absolute path.
			 */
			if (!pwd || strlen(pwd) == 0)
				pwd = "/";

			/*
			 * Use argument path if absolute
			 */
			if (absolute(path))
				copy_cstring(_path, path, _path_max_len);

			/*
			 * Otherwise, concatenate current working directory with
			 * relative path.
			 */
			else {
				const char *const relative_path = path;

				copy_cstring(_path, pwd, _path_max_len);

				if (!empty(relative_path)) {

					/* make sure to have a slash separating both portions */
					_append_slash_if_needed();
					_append(relative_path);
				}
			}
			_canonicalize();
		}

		Path_base(char *buf, size_t buf_len,
		          char const *path, char const *pwd = 0)
		:
			_path(buf), _path_max_len(buf_len)
		{
			import(path, pwd);
		}

		char       *base()         { return _path; }
		char const *base()   const { return _path; }
		char const *string() const { return _path; }

		size_t max_len() const { return _path_max_len; }

		void remove_trailing(char c) { remove_trailing(c, _path); }

		void keep_only_last_element()
		{
			char *dst = _path;
			for (char const *src = last_element(_path) ; *src; )
				*dst++ = *src++;

			*dst = 0;
		}

		void strip_last_element()
		{
			char *p = last_element(_path);
			if (p)
				p[p == _path ? 1 : 0] = 0;
		}

		bool equals(Path_base const &ref) const { return strcmp(ref._path, _path) == 0; }

		bool equals(char const *str) const { return strcmp(str, _path) == 0; }

		bool strip_prefix(char const *prefix)
		{
			size_t prefix_len = strlen(prefix);

			if (strcmp(prefix, _path, prefix_len) != 0)
				return false;

			if (prefix_len > 0 && ends_with('/', prefix))
				prefix_len--;

			if (prefix[prefix_len] && prefix[prefix_len] != '/')
				return false;

			if (_path[prefix_len] && _path[prefix_len] != '/')
				return false;

			_strip_from_begin(prefix_len);
			return true;
		}

		bool has_single_element() const
		{
			/* count number of non-trailing slashes */
			unsigned num_slashes = 0;
			for (char const *p = _path; *p; p++)
				num_slashes += (p[0] == '/' && p[1] != 0);

			/*
			 * Check if the leading slash is the only one, also check the
			 * absence of any element.
			 */
			return (num_slashes == 1) && !equals("/");
		}

		void append(char const *str) { _append(str); _canonicalize(); }

		void append_element(char const *str)
		{
			_append("/"); _append(str);
			_canonicalize();
		}

		bool operator == (char const *other) const
		{
			return strcmp(_path, other) == 0;
		}

		bool operator != (char const *other) const
		{
			return strcmp(_path, other) != 0;
		}

		bool operator == (Path_base const &other) const
		{
			return strcmp(_path, other._path) == 0;
		}

		bool operator != (Path_base const &other) const
		{
			return strcmp(_path, other._path) != 0;
		}

		char const *last_element() const
		{
			return last_element(_path)+1;
		}

		/**
		 * Print path to output stream
		 */
		void print(Genode::Output &output) const { output.out_string(base()); }
};


template <unsigned MAX_LEN>
class Genode::Path : public Path_base
{
	private:

		char _buf[MAX_LEN];

	public:

		/**
		 * Default constuctor
		 *
		 * Initializes the path to '/'.
		 */
		Path() : Path_base(_buf, sizeof(_buf), "/") { }

		/**
		 * Constructor
		 */
		Path(char const *path, char const *pwd = 0)
		: Path_base(_buf, sizeof(_buf), path, pwd) { }

		/**
		 * Constructor that implicitly imports a 'String'
		 */
		template <size_t N>
		Path(String<N> const &string)
		: Path_base(_buf, sizeof(_buf), string.string(), nullptr) { }

		static constexpr size_t capacity() { return MAX_LEN; }

		Path& operator=(char const *path)
		{
			copy_cstring(_buf, path, MAX_LEN);
			_canonicalize();
			return *this;
		}

		Path(Path const &other) : Path_base(_buf, sizeof(_buf), other._buf) { }

		Path& operator=(Path const &other)
		{
			copy_cstring(_buf, other._buf, MAX_LEN);
			return *this;
		}

		template <unsigned N>
		Path& operator=(Path<N> const &other)
		{
			copy_cstring(_buf, other._buf, MAX_LEN);
			return *this;
		}
};

namespace Genode {

	template <typename PATH>
	static inline PATH path_from_label(char const *label)
	{
		PATH path;

		char tmp[path.capacity()];
		memset(tmp, 0, sizeof(tmp));

		size_t len = strlen(label);

		size_t i = 0;
		for (size_t j = 1; j < len; ++j) {
			if (!strcmp(" -> ", label+j, 4)) {
				path.append("/");

				copy_cstring(tmp, label+i, (j-i)+1);
				/* rewrite any directory separators */
				for (size_t k = 0; k < path.capacity(); ++k)
					if (tmp[k] == '/')
						tmp[k] = '_';
				path.append(tmp);

				j += 4;
				i = j;
			}
		}
		path.append("/");
		copy_cstring(tmp, label+i, path.capacity());
		/* rewrite any directory separators */
		for (size_t k = 0; k < path.capacity(); ++k)
			if (tmp[k] == '/')
				tmp[k] = '_';
		path.append(tmp);

		return path;
	}
}

#endif /* _INCLUDE__OS__PATH_H_ */
