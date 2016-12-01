#ifndef _UTIL_H
#define _UTIL_H

#include <file_system_session/connection.h>

namespace File_system {

	struct Node_handle_guard
	{
		Connection *fs;
		Node_handle handle;

		Node_handle_guard(Connection *fs, Node_handle handle): fs(fs), handle(handle) { }

		~Node_handle_guard()
		{
			fs->close(handle);
		}
	};

	inline void remove_trailing_slash(char *path)
	{
		int i = 0;
		while (path[i] && path[i + 1]) i++;

		/*
		 * Never touch the first character to preserve the invariant of
		 * the leading slash.
		 */
		if (i > 0 && path[i] == '/')
			path[i] = 0;
	}
	
// 	inline char *last_element(char *path)
// 	{
// 		char *result = path;
// 		for (; *path; path++)
// 			if (path[0] == '/' && path[1] != 0)
// 				result = path;
// 		return result;
// 	}

	inline char *next_element(char *src, char *dest)
	{
		/* TODO: add code */
	}

	inline size_t subpath(const char *base, const char *sub)
	{
		size_t cnt = 0;
		for (; *base && *base == *sub; base++, sub++, cnt++);
		if (*base == 0 &&(*sub == '/' || *sub == 0))
			return cnt;
		else
			return 0;
	}

	inline bool valid_path(char const *path)
	{
		return (path && path[0] == '/')? true : false;
	}
};

#endif
