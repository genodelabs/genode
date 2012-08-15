/*
 * \brief  Path handling utility
 * \author Norman Feske
 * \date   2011-02-17
 */

/*
 * Copyright (C) 2011-2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__OS__PATH_H_
#define _INCLUDE__OS__PATH_H_

/* Genode includes */
#include <util/string.h>

namespace Genode {

	class Path_base
	{
		public:

			class Path_too_long { };

		protected:

			static bool is_absolute(char const *path)
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

			static bool is_empty(char const *path)
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

					/* strip superfluous dots, e.g., "/abs/./path/" -> "/abs/path" */
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

					/* strip superfluous slashes, e.g., "//path/" -> "/path" */
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

			static void strip(char *dst, unsigned count)
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

				strncpy(_path + orig_len, path, _path_max_len - orig_len);
			}

			void _append_slash_if_needed()
			{
				if (!ends_with('/', _path))
					_append("/");
			}

			void _strip_from_begin(unsigned count) { strip(_path, count); }

			/**
			 * Remove superfluous artifacts from absolute path
			 */
			void _canonicalize()
			{
				strip_superfluous_dotslashes(_path);
				strip_double_dot_dirs(_path);
				strip_superfluous_slashes(_path);
				remove_trailing('.', _path);
			}

			void _import(char const *path, char const *pwd = 0)
			{
				/*
				 * Validate 'pwd' argument, if not supplied, enforce invariant
				 * that 'pwd' is an absolute path.
				 */
				if (!pwd || strlen(pwd) == 0)
					pwd = "/";

				/*
				 * Use argument path if absolute
				 */
				if (is_absolute(path))
					strncpy(_path, path, _path_max_len);

				/*
				 * Otherwise, concatenate current working directory with
				 * relative path.
				 */
				else {
					const char *const relative_path = path;

					strncpy(_path, pwd, _path_max_len);

					if (!is_empty(relative_path)) {

						/* make sure to have a slash separating both portions */
						_append_slash_if_needed();
						_append(relative_path);
					}
				}
				_canonicalize();
			}

		public:

			Path_base(char *buf, size_t buf_len,
			          char const *path, char const *pwd = 0)
			:
				_path(buf), _path_max_len(buf_len)
			{
				_import(path, pwd);
			}

			void import(char const *path) { _import(path); }

			char  *base() { return _path; }
			size_t max_len() { return _path_max_len; }

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
				last_element(_path)[1] = 0;
			}

			bool equals(Path_base const &ref) const { return strcmp(ref._path, _path) == 0; }

			bool equals(char const *str) const { return strcmp(str, _path) == 0; }

			bool strip_prefix(char const *prefix)
			{
				unsigned prefix_len = strlen(prefix);

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
	};


	template <unsigned MAX_LEN>
	class Path : public Path_base {

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
	};
}

#endif /* _INCLUDE__OS__PATH_H_ */
